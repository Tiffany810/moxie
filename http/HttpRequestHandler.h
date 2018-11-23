#ifndef MOXIE_MCACHEDREQUESTHANDLER_H
#define MOXIE_MCACHEDREQUESTHANDLER_H
#include <iostream>
#include <map>
#include <functional>
#include <string>
#include <memory>

#include <moxie/base/ClientHandler.h>
#include <moxie/base/EventLoop.h>
#include <moxie/base/PollerEvent.h>
#include <moxie/base/NetAddress.h>
#include <moxie/http/HttpRequest.h>
#include <moxie/http/HttpResponse.h>

namespace moxie {

class HttpClientHandler : public ClientHandler {
public:
    HttpClientHandler(const std::shared_ptr<PollerEvent>& client,  const std::shared_ptr<moxie::NetAddress>& cad);
    void SetMethods(const std::map<std::string, std::function<void (moxie::HttpRequest&, moxie::HttpResponse&)>>& method);
    virtual void AfetrRead(const std::shared_ptr<PollerEvent>& event, EventLoop *loop) override;
    virtual void AfetrWrite(const std::shared_ptr<PollerEvent>& event, EventLoop *loop) override;
private:
    void ReplyErrorRequest(const string& error);
    bool ParseHttpRequest();
private:
    std::shared_ptr<PollerEvent> event_;
    std::shared_ptr<moxie::NetAddress> peer_;
    std::map<std::string, std::function<void (moxie::HttpRequest&, moxie::HttpResponse&)>> method_;
    HttpRequest request_;
    HttpResponse response_;
};

}

#endif 