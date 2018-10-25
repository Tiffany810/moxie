#ifndef MOXIE_CLIENT_H
#define MOXIE_CLIENT_H
#include <memory>

#include <PollerEvent.h>
#include <NetAddress.h>

namespace moxie {

class Client {
public:
    Client(std::string ip, short port, int32_t connect_timeout);
    ~Client();
    bool Connect();
    std::string GetIp() const;
    short GetPort() const;
    bool SetCloseExec();
    bool SetNonblock();
private:
    NetAddress addr_;
    int sock_;
    int32_t timeout_;
};

}

#endif 
