#ifndef MOXIE_HTTPSERVER_H
#define MOXIE_HTTPSERVER_H
#include <memory>
#include <unistd.h>

#include <moxie/base/EventLoop.h>
#include <moxie/base/PollerEvent.h>
#include <moxie/base/ListenHandler.h>
#include <moxie/base/NetAddress.h>
#include <moxie/http/HttpRequestHandler.h>
#include <moxie/http/HttpRequest.h>
#include <moxie/http/HttpResponse.h>

namespace moxie {

class HttpServer : public ListenHadler {
public:
    void RegisterMethodCallback(std::string cmd, std::function<void (moxie::HttpRequest&, moxie::HttpResponse&)> callback);
    virtual void AfterAcceptSuccess(const std::shared_ptr<PollerEvent>& client, EventLoop *loop, const std::shared_ptr<moxie::NetAddress>& cad) override;
private:
    std::map<std::string, std::function<void (moxie::HttpRequest&, moxie::HttpResponse&)>> method_;
};

}

#endif 
