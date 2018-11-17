#ifndef MOXIE_MCACHEDREQUESTHANDLER_H
#define MOXIE_MCACHEDREQUESTHANDLER_H
#include <iostream>
#include <vector>
#include <string>
#include <memory>

#include <ClientHandler.h>
#include <EventLoop.h>
#include <PollerEvent.h>
#include <NetAddress.h>
#include <EventLoopThread.h>

namespace moxie {

class McachedServer;

class McachedClientHandler : public ClientHandler, public std::enable_shared_from_this<McachedClientHandler> {
public:
    McachedClientHandler(McachedServer *server, const std::shared_ptr<PollerEvent>& client,  const std::shared_ptr<moxie::NetAddress>& cad);
    virtual void AfetrRead(const std::shared_ptr<PollerEvent>& event, EventLoop *loop) override;
    virtual void AfetrWrite(const std::shared_ptr<PollerEvent>& event, EventLoop *loop) override;
    static void MraftCallBack(const std::string& response, std::shared_ptr<McachedClientHandler> client);
private:
    class ResetArgcArgv {
    public:
        ResetArgcArgv(size_t& argc, std::vector<std::string>& argv) 
            : argc_(argc),
            argv_(argv) {}
        ~ResetArgcArgv() {
            argc_ = 0;
            argv_.clear();
        }
    private:
            size_t& argc_;
            std::vector<std::string>& argv_;
    };
    bool DoFinallyMcached(EventLoop *loop, const std::string& response);
    bool DoMcachedCammand(EventLoop *loop);
    void ApplyMcachedCommand();
    bool ParseRedisRequest();
    void DebugArgcArgv() const;
    void ReplyString(const std::string& error);
    void ReplyBulkString(const std::string& error);
private:
    McachedServer *server_;
    std::shared_ptr<PollerEvent> event_;
    std::shared_ptr<moxie::NetAddress> peer_;
    std::string cmd_;
    size_t argc_;
    std::vector<std::string> argv_;
    int curArgvlen_;
    std::shared_ptr<EventLoopThread> et_;
};

}

#endif 