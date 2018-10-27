#ifndef MOXIE_SERVICEMETA_H
#define MOXIE_SERVICEMETA_H
#include <list>
#include <stdint.h>

#include <Mutex.h>

namespace moxie {

class ServiceMeta {
public:
    struct SlotInfo {
        uint32_t index;
        bool is_adjust;
        uint64_t group_id;
        uint64_t dst_group_id;
    };

    static ServiceMeta* Instance();
    uint64_t CacheId();
    void CacheId(uint64_t groupid);
private:
    ServiceMeta() :
        group_id_(0) {
    }
    ServiceMeta(const ServiceMeta& that) = default;
    ServiceMeta& operator=(const ServiceMeta& that);
private:
    static ServiceMeta *instance_;
    Mutex mutex_;
    uint64_t group_id_;
    std::list<SlotInfo> moving_in_slots_;
    std::list<SlotInfo> moving_out_slots_;
};

}

#endif 