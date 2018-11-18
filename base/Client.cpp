#include <sys/epoll.h>
#include <string.h>

#include <Client.h>
#include <Epoll.h>
#include <Socket.h>

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

moxie::Client::Client(std::string ip, short port, int32_t connect_timeout) 
    : addr_(AF_INET, port, ip.c_str()),
     sock_(::socket(AF_INET, SOCK_STREAM, 0)),
     timeout_(connect_timeout) {
}

moxie::Client::~Client() { 
    ::close(sock_);
}

bool moxie::Client::Connect() {
    if(true == Socket::Connect(sock_, addr_) && errno != EINPROGRESS) {
        return true;
    }

    Epoll poll;
    struct epoll_event et;
    et.events = kReadEvent;
    et.data.fd = sock_;

    poll.EventAdd(sock_, &et);

    int ret = poll.LoopWait(&et, 1, timeout_);
    if (ret <= 0) {
        return false;
    }

    return checkConnectSucc(sock_);
}

std::string moxie::Client::GetIp() const {
    return addr_.getIp();
}

short moxie::Client::GetPort() const {
    return addr_.getPort();
}

bool moxie::Client::SetCloseExec() {
    return Socket::SetExecClose(this->sock_);
}

bool moxie::Client::SetNonblock() {
    return Socket::SetNoBlocking(sock_);
}
