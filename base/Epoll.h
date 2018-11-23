#ifndef MOXIE_EPOLL_H
#define MOXIE_EPOLL_H
#include <sys/epoll.h>
#include <vector>
#include <unistd.h>

#include <moxie/base/Timestamp.h>
#include <moxie/base/PollerEvent.h>

namespace moxie {

class Epoll {
public:
    Epoll();
    ~Epoll() {
        ::close(epoll_fd_);
    }

    bool EventAdd(int fd, struct epoll_event* event);
    bool EventDel(int fd, struct epoll_event* event);
    bool EventMod(int fd, struct epoll_event* event);
    int LoopWait(struct epoll_event* events, int maxevents, int timeout);
    bool EventCtl(int op, int fd, struct epoll_event* event);
private:
    int epoll_fd_;
};

}
#endif // MOXIE_EPOLL_H
