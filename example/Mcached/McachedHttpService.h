#ifndef MOXIE_MCACHEDHTTPSERVICE_H
#define MOXIE_MCACHEDHTTPSERVICE_H
#include <memory>

#include <curl/curl.h>

#include <HttpServer.h>
#include <HttpRequest.h>
#include <HttpResponse.h>
#include <NetAddress.h>
#include <PollerEvent.h>
#include <EventLoop.h>
#include <Thread.h>

namespace moxie {

struct HttpServiceConf {
    std::string ip;
    short port;
    std::string work_path;
    std::string etcd_server_list;
};

class McachedHttpService {
public:
    McachedHttpService();
    bool Init(const HttpServiceConf& conf);
    bool Start();
private:
    int GetByCurl(const string& url, string* response);
    int PostByCurl(const string& url, string &body,  string* response);
    void ThreadWorker();
    void PostProcess(HttpRequest& request, HttpResponse& response);
private:
    HttpServiceConf conf_;
    std::shared_ptr<HttpServer> server_;
    NetAddress addr_;
    std::shared_ptr<PollerEvent> event_;
    EventLoop *loop_;
    std::shared_ptr<Thread> thread_;
};

}

#endif