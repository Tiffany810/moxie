#ifndef MOXIE_WAKEUPER_H
#define MOXIE_WAKEUPER_H
#include <sys/eventfd.h>

#include <Handler.h>

namespace mxoie {
    
class Wakeuper : public Handler {
public:
    Wakeuper() 
        : Wakeup_(::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC)) {
         assert(this->wakeUpFd_ > 0);
    }
    virtual ~Wakeuper() {}
protected:
    void WakeUp() {
        uint64_t one = 1;
        ssize_t n = ::write(wakeUpFd_, &one, sizeof one);
        if (n != sizeof one) {
            LOGGER_ERROR("EventLoop::wakeup() writes " << n << " bytes instead of 8");
        }
    }
    void ClearWake() {
        uint64_t one = 1;
        ssize_t n = ::read(wakeUpFd_, &one, sizeof one);
        if (n != sizeof one){
            LOGGER_ERROR("EventLoop::handleRead() reads " << n << " bytes instead of 8");
        }
    }
private:
    int Wakeup_;
};

} // mxoie



#endif MOXIE_WAKEUPER_H