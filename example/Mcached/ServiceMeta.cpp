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

bool moxie::ServiceMeta::CacheIdActivated(bool activated) {
    MutexLocker lock(mutex_);
    this->group_id_activated_ = activated;
}