#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/tcp.h>

#include <moxie/base/Socket.h>
#include <moxie/base/Log.h>
#include <moxie/base/NetAddress.h>
#include <moxie/base/Epoll.h>

bool moxie::Socket::SetNoBlocking(int sock) {
    int flag = ::fcntl(sock, F_GETFL);
    flag |= O_NONBLOCK;
    int ret = ::fcntl(sock, F_SETFL, flag);
    if (ret == -1) {
        LOGGER_SYSERR("fcntl error : " << ::strerror(errno));
        return false;
    }
    return true;
}

bool moxie::Socket::SetExecClose(int sock) {
    int flag = ::fcntl(sock, F_GETFD, 0);
    flag |= FD_CLOEXEC;
    int ret = ::fcntl(sock, F_SETFD, flag);
    if (ret == -1) {
        LOGGER_SYSERR("fcntl error : " << ::strerror(errno));
        return false;
    }
    return true;
}

bool moxie::Socket::Bind(int sock, const NetAddress& addr) {
    int ret = ::bind(sock, addr.addrPtr(), addr.addrLen());
    if (ret == -1) {
        LOGGER_SYSERR("bind error : " << ::strerror(errno));
        return false;
    }
    return true;
}

bool moxie::Socket::Listen(int sock, int backlog) {
    int ret = ::listen(sock, backlog);
    if (ret == -1) {
        LOGGER_SYSERR("listen error : " << ::strerror(errno));
        return false;
    }
    return true;
}

bool moxie::Socket::Connect(int sock, const NetAddress& addr) {
    int ret = ::connect(sock, addr.addrPtr(), addr.addrLen());
    if (ret == -1) {
        if (errno == EINPROGRESS) {
            return true;
        }
        LOGGER_SYSERR("connect error : " << ::strerror(errno));
        return false;
    }
    return true;
}

bool checkConnectSucc(int sd) {
    int error = 0;
    socklen_t len = sizeof(error);
    if (getsockopt(sd, SOL_SOCKET, SO_ERROR,
                reinterpret_cast<char *>(&error), &len) == 0) {
        if (error != 0) {
            LOGGER_ERROR(" getsockopt : " << ::strerror(error));
            return false;
        }
        return true;
    } else {
        LOGGER_ERROR(" getsockopt : " << ::strerror(errno));
        return false;
    } 
}

bool moxie::Socket::Connect(int sock, const NetAddress& addr, int64_t ms) {
    if(true == Socket::Connect(sock, addr) && errno != EINPROGRESS) {
        return true;
    }

    Epoll poll;
    struct epoll_event et;
    et.events = kReadEvent;
    et.data.fd = sock;

    poll.EventAdd(sock, &et);

    int ret = poll.LoopWait(&et, 1, ms);
    if (ret <= 0) {
        return false;
    }

    return checkConnectSucc(sock);
}

int moxie::Socket::Accept(int sock, moxie::NetAddress& addr, bool noblockingexec) {
    int flags = 0;
    if (noblockingexec) {
        flags = SOCK_NONBLOCK | SOCK_CLOEXEC;
    }
    socklen_t len = addr.addrLen();
    int ret = ::accept4(sock, addr.addrPtr(), &len, flags);
    if(ret == -1) {
        LOGGER_SYSERR("accept error : " << ::strerror(errno));
        return ret;
    }
    return ret;
}

int moxie::Socket::Accept(int sock, bool noblockingexec) {
    int flags = 0;
    if (noblockingexec) {
        flags = SOCK_NONBLOCK | SOCK_CLOEXEC;
    }
    int ret = ::accept4(sock, nullptr, nullptr, flags);
    if(ret == -1) {
        LOGGER_SYSERR("accept error : " << ::strerror(errno));
        return ret;
    }
    return ret;
}

bool moxie::Socket::SetTcpNodelay(int sock) {
    int enable = 1;
    if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (void*)&enable, sizeof(enable)) < 0) {
        LOGGER_SYSERR("setsockopt error : " << ::strerror(errno));
        return false;
    }
    return true;
}

bool moxie::Socket::SetReusePort(int sock) {
    int enable = 1;
    if (setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, (void*)&enable, sizeof(enable)) < 0) {
        LOGGER_SYSERR("setsockopt error : " << ::strerror(errno));
        return false;
    }
    return true;
}
