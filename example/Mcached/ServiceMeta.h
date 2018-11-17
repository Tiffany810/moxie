#ifndef MOXIE_SERVICEMETA_H
#define MOXIE_SERVICEMETA_H
#include <list>
#include <stdint.h>
#include <unordered_map>

#include <Mutex.h>

namespace moxie {

class ServiceMeta {
public:
    struct SlotInfo {
        uint32_t index;
        bool is_adjust;
        uint64_t source_id;
        uint64_t dest_id;
    };

    enum MetaDataError {
        MetaOk = 0,
        SlotsExists,
        SlotsNotExists,
        GroupIdIsInvaild,
        SourceIdIsInvaild,
        SourceIdIsIncorrect,
        DestIdIsIncorrect,
        SlotInMoveOut,
        SlotNotInMoveOut,
        SlotInMoveIn,
        SlotNotInMoveIn,
        SlotNotStartMove,
    };

    static ServiceMeta* Instance();
    uint64_t CacheId();
    void CacheId(uint64_t groupid);
    bool CacheIdActivated();
    void CacheIdActivated(bool activated);

    int AddSlot(uint32_t slot);
    int DelSlot(uint32_t slot);
    int StartMoveOutSlot(uint32_t slot, uint64_t dest_id);
    int DoneMoveOutSlot(uint32_t slot, uint64_t dest_id);

    int StartMoveInSlot(uint32_t slot, uint64_t src_id);
    int DoneMoveInSlot(uint32_t slot, uint64_t src_id);
private:
    ServiceMeta() :
        group_id_(0),
        group_id_activated_(false) {
    }
    ServiceMeta(const ServiceMeta& that) = default;
    ServiceMeta& operator=(const ServiceMeta& that);
private:
    static ServiceMeta *instance_;
    Mutex mutex_;
    uint64_t group_id_;
    bool group_id_activated_;
    std::unordered_map<uint32_t, SlotInfo*> slots_;
    std::unordered_map<uint32_t, SlotInfo*> moving_in_slots_;
    std::unordered_map<uint32_t, SlotInfo*> moving_out_slots_;
};

}

#endif 