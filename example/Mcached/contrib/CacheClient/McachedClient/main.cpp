#include <atomic>

#include <libmemcached/memcached.h>

#include <McachedClient.h>

struct Context {
    int id;
    int datalen;
    int reqs;
};

std::atomic_llong total_set_time(0);
std::atomic_llong total_get_time(0);
std::atomic_llong total_reqs(0);

void* BenchMark(void *data) {
    struct Context *ctx = static_cast<struct Context *>(data);
    std::string keyprefix, valueprefix;
    keyprefix.resize(50, 'k');
    valueprefix.resize(ctx->datalen, 'v');

    mmcache::McachedClient client;
    client.Init("192.168.56.1:11211");
    int rq = 0;
    LOG(INFO) << "Begin Set reqs=" << ctx->reqs << " datalen=" << ctx->datalen;
    std::chrono::steady_clock::time_point start_set = std::chrono::steady_clock::now();
    for (rq = 0; rq < ctx->reqs; ++rq) {
        std::string key = std::to_string(ctx->id) + keyprefix + std::to_string(rq);
        std::string value = std::to_string(ctx->id) + valueprefix + std::to_string(rq);
        if (!client.Set(key, value)) {
            LOG(ERROR) << "Set key:" << key << " failed!";
            break;
        }
    }
    std::chrono::steady_clock::time_point end_set = std::chrono::steady_clock::now();
    LOG(INFO) << "Begin Get rq=" << rq;
    for (int i = 0; i < rq; ++i) {
        std::string key = std::to_string(ctx->id) + keyprefix + std::to_string(i);
        uint32_t flags;
        std::string value;
        if (!client.Get(key, &flags, value)) {
            LOG(ERROR) << "Get key:" << key << " failed!";
            break;
        }
    }
    std::chrono::steady_clock::time_point end_get = std::chrono::steady_clock::now();
    LOG(INFO) << "End Get!";

    total_set_time += std::chrono::duration_cast<std::chrono::microseconds>(end_set - start_set).count();
    total_get_time += std::chrono::duration_cast<std::chrono::microseconds>(end_get - end_set).count();
    total_reqs += rq;

    delete ctx;
    return nullptr;
}

size_t InitMcachedData(const std::string& server, size_t reqs, size_t datalen) {
    mmcache::McachedClient client;
    client.Init(server, "");
    int rq = 0;
    LOG(INFO) << "Begin init mcached data =" << reqs << " datalen=" << datalen;
    std::string keyprefix, valueprefix;
    keyprefix.resize(50, 'k');
    valueprefix.resize(datalen, 'v');
    for (rq = 0; rq < reqs; ++rq) {
        std::string key = keyprefix + std::to_string(rq);
        std::string value = valueprefix + std::to_string(rq);
        if (!client.Set(key, value)) {
            LOG(ERROR) << "Set key:" << key << " failed!";
            break;
        }
    }
    return rq;
}

bool GetDataUseMcachedClient(const std::string& server_list, const std::string& lb, size_t times, size_t reqs, size_t datalen) {
    mmcache::McachedClient client;
    client.Init(server_list, lb);
    std::string keyprefix;
    keyprefix.resize(50, 'k');

    std::chrono::steady_clock::time_point start_get = std::chrono::steady_clock::now();
    size_t total_reqs = 0;
    for (size_t kk = 0; kk < times; ++kk) {
        int rq = 0;
        for (rq = 0; rq < reqs; ++rq) {
            std::string key = keyprefix + std::to_string(rq);
            uint32_t flags;
            std::string value;
            if (!client.Get(key, &flags, value)) {
                LOG(ERROR) << "Get key:" << key << " failed!";
                break;
            }
        }
        total_reqs += rq;
    }
    std::chrono::steady_clock::time_point end_get = std::chrono::steady_clock::now();
    uint64_t total_get_time = std::chrono::duration_cast<std::chrono::microseconds>(end_get - start_get).count();
    std::cout << "balance:" << lb << " reqs:" << total_reqs << " datalen:" << datalen << " total_time:" << total_get_time << std::endl;
    return total_reqs;
}

size_t GetDataUseLibMcachedClient(const std::vector<std::pair<std::string, short>>& ser_list, const std::string& lb, size_t times, size_t reqs, size_t datalen) {
    memcached_st *memc;   
    memcached_return rc;   
    memcached_server_st *servers;   

    memc = memcached_create(NULL);   

    for (size_t i = 0; i < ser_list.size(); ++i) {
        if (i == 0) {
            servers = memcached_server_list_append(NULL, (char *)ser_list[i].first.c_str(), ser_list[i].second, &rc);
        } else {
            servers = memcached_server_list_append(servers, (char *)ser_list[i].first.c_str(), ser_list[i].second, &rc);
        }
    }

    rc = memcached_server_push(memc, servers);  
    memcached_server_free(servers);    
    if (lb == "random") {
        memcached_behavior_set(memc, MEMCACHED_BEHAVIOR_DISTRIBUTION, MEMCACHED_DISTRIBUTION_RANDOM); 
    } else if (lb == "chash") {
        memcached_behavior_set(memc, MEMCACHED_BEHAVIOR_DISTRIBUTION, MEMCACHED_DISTRIBUTION_CONSISTENT); 
    } else {
        assert(false);
    }
    std::string keyprefix;
    keyprefix.resize(50, 'k');
    std::chrono::steady_clock::time_point start_get = std::chrono::steady_clock::now();
    size_t total_reqs = 0;
    for (size_t kk = 0; kk < times; ++kk) {
        int rq = 0;
        for (rq = 0; rq < reqs; ++rq) {
            std::string key = keyprefix + std::to_string(rq);
            size_t value_length = 0;
            uint32_t flags = 0;
            char* result = memcached_get(memc, key.c_str(), key.size(), &value_length, &flags, &rc);
            if(rc != MEMCACHED_SUCCESS) {
                LOG(ERROR) << "Get key:" << key << " failed!";        
            }
        }
        total_reqs += rq;
    }

    std::chrono::steady_clock::time_point end_get = std::chrono::steady_clock::now();
    uint64_t total_get_time = std::chrono::duration_cast<std::chrono::microseconds>(end_get - start_get).count();
    std::cout << "balance:" << lb << " reqs:" << total_reqs << " datalen:" << datalen << " total_time:" << total_get_time << std::endl;
    return total_reqs;
}

int BenchMarkMain (int argc, char** argv) {
    if (argc < 4) {
        LOG(ERROR) << "Usage:exec datalen reqs thread";
        return -1;
    }

    int datalen = std::atoi(argv[1]);
    int reqs = std::atoi(argv[2]);
    int thread = std::atoi(argv[3]);

    if (datalen <= 0 || reqs <= 0 || thread <= 0) {
        LOG(ERROR) <<  "datalen <= 0 || reqs <= 0 || thread <= 0";
        return -1;
    }

    std::vector<bthread_t> bids;
    bids.resize(thread);
    for (int i = 0; i < thread; ++i) {
        struct Context *ctx = new struct Context;
        ctx->datalen = datalen;
        ctx->reqs = reqs;
        ctx->id = i;
        if (bthread_start_background(&bids[i], NULL, BenchMark, ctx) != 0) {
            LOG(ERROR) << "Fail to create bthread";
            return -1;
        }
    }

    for (int i = 0; i < thread; ++i) {
        bthread_join(bids[i], NULL);
    }

    double set_per_sec = (double)total_reqs / (double)total_set_time * 1000000;
    double get_per_sec = (double)total_reqs / (double)total_get_time * 1000000;
    std::cout << "total_reqs=[" << total_reqs << "] set_per_sec=[" << set_per_sec << "] get_per_sec=[" << get_per_sec << "]" << std::endl;
    return 0;
}

int main (int argc, char** argv) {
    if (argc < 3) {
        LOG(ERROR) << "Usage:exec datalen reqs";
        return -1;
    }

    int datalen = std::atoi(argv[1]);
    int reqs = std::atoi(argv[2]);

    if (datalen <= 0 || reqs <= 0) {
        LOG(ERROR) <<  "datalen <= 0 || reqs <= 0";
        return -1;
    }

    std::string server_first = "192.168.56.1:8090";
    std::string server_second = "192.168.56.2:8091";

    size_t rq_first = InitMcachedData(server_first, reqs, datalen);
    size_t rq_second = InitMcachedData(server_second, reqs, datalen);
    if (rq_first != rq_second || rq_first < 0) {
        std::cout << "InitMcachedData failed!" << std::endl;
        return -1;
    }

    std::cout << "Init data items:" << rq_first << std::endl;

    std::string server_list = "list://" + server_first + "," + server_second;
    for (size_t i = 1; i <= 5; ++i) {
        GetDataUseMcachedClient(server_list, "la", i, rq_first, datalen);
    }

    std::cout << "\n\n";

    std::vector<std::pair<std::string, short>> ser_list = {
        { "192.168.56.1", 8090 },
        { "192.168.56.2", 8091 },
    };
    for (size_t i = 1; i <= 5; ++i) {
        GetDataUseLibMcachedClient(ser_list, "random", i, rq_first, datalen);
    }

    for (size_t i = 1; i <= 5; ++i) {
        GetDataUseLibMcachedClient(ser_list, "chash", i, rq_first, datalen);
    }

    return 0;
}

