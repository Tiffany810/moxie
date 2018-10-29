#include <atomic>

#include <MemcachedClient.h>

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

    mmcache::MemcachedClient client;
    client.Init("192.168.56.1:11211", 500, 3);
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

int main (int argc, char** argv) {
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

