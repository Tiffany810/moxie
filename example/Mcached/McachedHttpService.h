#ifndef MOXIE_MCACHEDHTTPSERVICE_H
#define MOXIE_MCACHEDHTTPSERVICE_H
#include <memory>
#include <string>

#include <curl/curl.h>

#include <HttpServer.h>
#include <HttpRequest.h>
#include <HttpResponse.h>
#include <NetAddress.h>
#include <PollerEvent.h>
#include <EventLoop.h>
#include <Thread.h>
#include <Timer.h>

namespace moxie {

struct HttpServiceConf {
    std::string ip;
    short port;
    std::string work_path;
    std::string manager_server_list;
    std::string keepalive_server_list;
    std::string mcached_hosts;
};

class McachedHttpService {
public:
    enum CmdReverseType {
        CmdCreateCacheGroup 		= 	0,
        CmdActivateGroup			= 	1,
        CmdDeleteCacheGroup			= 	2,
        CmdAddSlotToGroup			= 	3,
        CmdMoveSlotDone				= 	4,
        CmdDelSlotFromGroup			= 	5,
        CmdMoveSlotStart			= 	6,
        CmdPeekGroupHosts           =   7,
    };

    enum CmdKeepAliveType{
        GroupKeepAlive				= 	0,
    };

    struct GroupReviseRequest {
	    uint32_t cmd_type;
	    uint64_t source_id;
	    uint64_t dest_id;
	    uint64_t slot_id;
    };

    struct GroupReviseResponse {
        bool succeed;
        int32_t ecode;
        std::string msg;
    };

    struct CurlExt {
        long status_code;
        std::string redirect_url;
    };

    enum SlotRange {
        McachedSlotsStart           =   1,
        McachedSlotsEnd				=   1025,
    };

    struct ClientRequestArgs {
        int32_t cmd_type;
        int32_t slot_id;
        int32_t group_id;
    };

    enum CmdFromClient {
        CmdAddSlot                  =   0,
        CmdDelSlot                  =   1,
        CmdMoveSlot                 =   2,
    };

    McachedHttpService();
    ~McachedHttpService();
    bool Init(const HttpServiceConf& conf);
    bool Start();
private:
    int GetByCurl(const std::string& url, std::string& response, struct CurlExt &ext);
    int PostByCurl(const std::string& url, std::string &body, 
                   std::string& response, struct CurlExt &ext);
    
    void CreateCachedId();
    void ActivatedCachedId();

    void ThreadWorker();

    void CheckerThreadWorker();
    void CheckerTimerWorker(moxie::Timer *timer, moxie::EventLoop *loop);
    void KeepAliveTimerWorker(moxie::Timer *timer, moxie::EventLoop *loop);
    void KeepAlive();

    void PostProcess(HttpRequest& request, HttpResponse& response);
    void GetProcess(HttpRequest& request, HttpResponse& response);
    void Http4xxResponse(HttpResponse& response,
                        const std::string& code,
                        const std::string& status,
                        const std::string& version);

    bool ParseClientRequestArgs(HttpRequest& request, ClientRequestArgs& args);
    void ProcessCmdAddSlot(const ClientRequestArgs& args, HttpRequest& request, HttpResponse& response);
    void ProcessCmdDelSlot(const ClientRequestArgs& args, HttpRequest& request, HttpResponse& response);
    void ProcessCmdMoveSlot(const ClientRequestArgs& args, HttpRequest& request, HttpResponse& response);
private:
    HttpServiceConf conf_;
    std::shared_ptr<HttpServer> server_;
    NetAddress addr_;
    std::shared_ptr<PollerEvent> event_;

    // for http service
    EventLoop *loop_;
    std::shared_ptr<Thread> thread_;

    std::string mcached_hosts_;

    // for meta checker
    EventLoop *checker_loop_;
    std::shared_ptr<Thread> checker_thread_;
    moxie::Timer *checker_timer_;
    moxie::Timer *keepalive_timer_;

    std::vector<std::string> manager_url_list_;
    std::string cur_manager_url_;

    std::vector<std::string> keepalive_server_list_;
    std::string cur_keepalive_url_;
};

}

#endif