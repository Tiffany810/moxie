#ifndef MEMCACHEDCLIENT_H
#define MEMCACHEDCLIENT_H
#include <bthread/bthread.h>
#include <butil/logging.h>
#include <butil/string_printf.h>
#include <brpc/channel.h>
#include <brpc/memcache.h>
#include <brpc/policy/couchbase_authenticator.h>

namespace mmcache {

class MemcachedClient {
public:
    bool Init(const std::string& server, int32_t timeout, int32_t retry);
    bool Set(const std::string& key, const std::string& value, uint32_t flags = 1111, uint32_t exptime = 0, uint64_t cas_value = 0);
    bool Get(const std::string& key, uint32_t *flags, std::string& value);
private:
    brpc::Channel channel;
};

}

#endif // MEMCACHEDCLIENT_H
