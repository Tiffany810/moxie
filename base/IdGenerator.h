#ifndef MOXIE_IDGENERATOR_H
#define MOXIE_IDGENERATOR_H

#include <moxie/base/Mutex.h>
#include <moxie/base/MutexLocker.h>
#include <moxie/base/Timestamp.h>

namespace moxie {

class IdGenerator {
public:
    IdGenerator(int64_t memberid, uint64_t nano);
    uint64_t Next();
private:
    uint64_t lowbit(uint64_t x, uint32_t n);
private:
    static const uint32_t kTsLen = 5 * 8;
    static const uint32_t kCntLen = 8;
    static const uint32_t kSuffixLen = kTsLen + kCntLen;
    uint64_t prefix_;
    uint64_t suffix_;
    Mutex mutex_;
};

}

#endif 