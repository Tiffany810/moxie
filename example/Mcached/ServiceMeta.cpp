#include <ServiceMeta.h>
#include <MutexLocker.h>

moxie::ServiceMeta* moxie::ServiceMeta::instance_ = new moxie::ServiceMeta();

moxie::ServiceMeta* moxie::ServiceMeta::Instance() {
    return moxie::ServiceMeta::instance_;
}

uint64_t moxie::ServiceMeta::CacheId() {
    moxie::MutexLocker lock(mutex_);
    return this->group_id_;
}

void moxie::ServiceMeta::CacheId(uint64_t groupid) {
    MutexLocker lock(mutex_);
    this->group_id_ = groupid;
}

bool moxie::ServiceMeta::CacheIdActivated() {
    MutexLocker lock(mutex_);
    return this->group_id_activated_;
}

void moxie::ServiceMeta::CacheIdActivated(bool activated) {
    MutexLocker lock(mutex_);
    this->group_id_activated_ = activated;
}

int moxie::ServiceMeta::AddSlot(uint32_t slot) {
    MutexLocker lock(mutex_);
    if (this->slots_.count(slot) == 1) {
        return SlotsExists;
    }

    if (this->moving_in_slots_.count(slot) == 1) {
        return SlotInMoveIn;
    }

    if (this->moving_out_slots_.count(slot) == 1) {
        return SlotInMoveOut;
    }

    if (group_id_ <= 0) {
        return GroupIdIsInvaild;
    }

    auto info = new SlotInfo;
    info->source_id = group_id_;
    info->index = slot;
    info->is_adjust = false;
    info->dest_id = 0;
    return MetaOk;
}

int moxie::ServiceMeta::DelSlot(uint32_t slot) {
    MutexLocker lock(mutex_);
    if (this->slots_.count(slot) == 0) {
        return SlotsNotExists;
    }

    if (this->moving_in_slots_.count(slot) == 1) {
        return SlotInMoveIn;
    }

    if (this->moving_out_slots_.count(slot) == 1) {
        return SlotInMoveOut;
    }

    if (group_id_ <= 0) {
        return GroupIdIsInvaild;
    }

    this->slots_.erase(slot);

    return MetaOk;
}

int moxie::ServiceMeta::StartMoveOutSlot(uint32_t slot, uint64_t dest_id) {
    MutexLocker lock(mutex_);
    if (this->slots_.count(slot) != 1) {
        return SlotsNotExists;
    }

    if (this->moving_in_slots_.count(slot) == 1) {
        return SlotInMoveIn;
    }

    if (this->moving_out_slots_.count(slot) == 1) {
        return SlotInMoveOut;
    }

    if (group_id_ <= 0) {
        return GroupIdIsInvaild;
    }

    auto info = this->slots_[slot];
    info->is_adjust = true;
    info->dest_id = dest_id;
    this->moving_out_slots_[slot] = info;
    this->slots_.erase(slot);

    return MetaOk;
}

int moxie::ServiceMeta::DoneMoveOutSlot(uint32_t slot, uint64_t dest_id) {
    MutexLocker lock(mutex_);
    if (this->slots_.count(slot) == 1) {
        return SlotNotStartMove;
    }

    if (this->moving_in_slots_.count(slot) == 1) {
        return SlotInMoveIn;
    }

    if (this->moving_out_slots_.count(slot) != 1) {
        return SlotNotInMoveOut;
    }

    if (group_id_ <= 0) {
        return GroupIdIsInvaild;
    }

    auto info = this->moving_out_slots_[slot];

    if (info->dest_id != dest_id) {
        return DestIdIsIncorrect;
    }

    info->is_adjust = false;
    info->dest_id = 0;
    this->slots_[slot] = info;
    this->moving_out_slots_.erase(slot);

    return MetaOk;
}

int moxie::ServiceMeta::StartMoveInSlot(uint32_t slot, uint64_t source_id) {
    MutexLocker lock(mutex_);
    if (this->slots_.count(slot) == 1) {
        return SlotsExists;
    }

    if (this->moving_in_slots_.count(slot) == 1) {
        return SlotInMoveIn;
    }

    if (this->moving_out_slots_.count(slot) == 1) {
        return SlotInMoveOut;
    }

    if (group_id_ <= 0) {
        return GroupIdIsInvaild;
    }

    if (source_id <= 0) {
        return SourceIdIsInvaild;
    }

    auto info = new SlotInfo;
    info->is_adjust = true;
    info->dest_id = group_id_;
    info->source_id = source_id;
    this->moving_in_slots_[slot] = info;

    return MetaOk;
}

int moxie::ServiceMeta::DoneMoveInSlot(uint32_t slot, uint64_t source_id) {
    MutexLocker lock(mutex_);
    if (this->slots_.count(slot) == 1) {
        return SlotsExists;
    }

    if (this->moving_in_slots_.count(slot) != 1) {
        return SlotNotInMoveIn;
    }

    if (this->moving_out_slots_.count(slot) == 1) {
        return SlotInMoveOut;
    }

    if (group_id_ <= 0) {
        return GroupIdIsInvaild;
    }

    if (source_id <= 0) {
        return SourceIdIsInvaild;
    }

    auto info = this->moving_in_slots_[slot];

    if (info->source_id != source_id) {
        return SourceIdIsIncorrect;
    }

    info->is_adjust = false;
    info->dest_id = 0;
    info->source_id = group_id_;
    this->slots_[slot] = info;
    this->moving_in_slots_.erase(slot);

    return MetaOk;
}