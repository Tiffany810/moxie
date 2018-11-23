#ifndef MOXIE_WAKEUPER_H
#define MOXIE_WAKEUPER_H
#include <sys/eventfd.h>
#include <assert.h>

#include <moxie/base/Handler.h>

namespace moxie {
    
class Wakeuper : virtual public Handler {
public:
    Wakeuper() 
        : wakeUpEvent_(new PollerEvent(::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC), kReadEvent)) {
         assert(this->wakeUpEvent_->GetFd() > 0);
    }
    virtual ~Wakeuper() {}
protected:
    void WakeUp() {
        uint64_t one = 1;
        ssize_t n = ::write(wakeUpEvent_->GetFd(), &one, sizeof one);
        if (n != sizeof one) {
            LOGGER_ERROR("EventLoop::wakeup() writes " << n << " bytes instead of 8");
        }
    }
    void ClearWake() {
        uint64_t one = 1;
        ssize_t n = ::read(wakeUpEvent_->GetFd(), &one, sizeof one);
        if (n != sizeof one){
            LOGGER_ERROR("EventLoop::handleRead() reads " << n << " bytes instead of 8");
        }
    }
    std::shared_ptr<PollerEvent> GetEvent() {
        return wakeUpEvent_;
    }
protected:
    std::shared_ptr<PollerEvent> wakeUpEvent_;
};

} // mxoie



#endif // MOXIE_WAKEUPER_H