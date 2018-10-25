#include <MemcachedClient.h>

bool mmcache::MemcachedClient::Init(const std::string& server, int32_t timeout, int32_t retry) {
    brpc::ChannelOptions options;
    options.protocol = brpc::PROTOCOL_MEMCACHE;
    options.connection_type = "single";
    options.timeout_ms = timeout /*milliseconds*/;
    options.max_retry = retry;

    if (channel.Init(server.c_str(), &options) != 0) {
        LOG(ERROR) << "Fail to initialize channel";
        return false;
    }
    return true;
}

bool mmcache::MemcachedClient::Set(const std::string& key, const std::string& value, uint32_t flags, uint32_t exptime, uint64_t cas_value) {
    brpc::MemcacheRequest request;
    if (!request.Set(key, value, flags, exptime, cas_value)) {
        LOG(ERROR) << "Fail to SET request";
        return false;
    }

    brpc::MemcacheResponse response;
    brpc::Controller cntl;

    channel.CallMethod(NULL, &cntl, &request, &response, NULL);
    if (cntl.Failed()) {
        LOG(ERROR) << "Fail to access memcache, " << cntl.ErrorText();
        return false;
    }
    if (!response.PopSet(NULL)) {
        LOG(ERROR) << "Fail to SET memcache " << response.LastError();
        return false;
    }
    return true;
}

bool mmcache::MemcachedClient::Get(const std::string& key, uint32_t* flags, std::string& value) {
    brpc::MemcacheRequest request;
    if (!request.Get(key)) {
        LOG(ERROR) << "Fail to GET request";
        return false;
    }

    brpc::MemcacheResponse response;
    brpc::Controller cntl;

    channel.CallMethod(NULL, &cntl, &request, &response, NULL);

    if (cntl.Failed()) {
        LOG(ERROR) << "Fail to access memcache, " << cntl.ErrorText();
        return false;
    }

    if (!response.PopGet(&value, flags, NULL)) {
        LOG(INFO) << "Fail to GET the key, " << response.LastError();
        return false;
    }
    return true;
}
