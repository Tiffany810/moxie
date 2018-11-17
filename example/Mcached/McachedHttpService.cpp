#include <functional>

#include <json/json.h>

#include <McachedHttpService.h>
#include <Socket.h>
#include <StringOps.h>
#include <ServiceMeta.h>

moxie::McachedHttpService::McachedHttpService()
    : server_(std::make_shared<HttpServer>()),
    loop_(new EventLoop),
    checker_loop_(new EventLoop) {
}

moxie::McachedHttpService::~McachedHttpService() {
    this->thread_->Stop();
    checker_thread_->Stop();
    delete loop_;
    delete checker_loop_;
}

bool moxie::McachedHttpService::Init(const Conf& conf) {
    this->serviceConf_ = conf.GetServiceConf();
    this->managerServiceConf_ = conf.GetManagerServiceConf();
    this->mcachedConf_ = conf.GetMcachedConf();

    int server = socket(AF_INET, SOCK_STREAM, 0);
    if (server < 0) {
        LOGGER_ERROR("socket error : " << strerror(errno));
        return false;
    }

    curl_global_init(CURL_GLOBAL_ALL);

    Socket::SetExecClose(server);
    Socket::SetNoBlocking(server);
    Socket::SetReusePort(server);

    addr_ = NetAddress(AF_INET, this->serviceConf_.port, this->serviceConf_.ip.c_str());

    if (!(Socket::Bind(server, addr_)
          && Socket::Listen(server, 128))) {
        return false;
    }
    if (this->managerServiceConf_.urls.size() > 0) {
        this->cur_manager_url_ = this->managerServiceConf_.urls[0];
    }

    event_ = std::make_shared<PollerEvent>(server, moxie::kReadEvent);
    server_->RegisterMethodCallback("POST", std::bind(&McachedHttpService::PostProcess, this, std::placeholders::_1, std::placeholders::_2));
    server_->RegisterMethodCallback("post", std::bind(&McachedHttpService::PostProcess, this, std::placeholders::_1, std::placeholders::_2));

    keepalive_timer_ = new Timer(std::bind(&McachedHttpService::KeepAliveTimerWorker, this, std::placeholders::_1, std::placeholders::_2), moxie::AddTime(Timestamp::Now(), 0.5), 2);
    checker_timer_ = new Timer(std::bind(&McachedHttpService::CheckerTimerWorker, this, std::placeholders::_1, std::placeholders::_2), moxie::AddTime(Timestamp::Now(), 0.5));

    thread_ = std::make_shared<Thread>(std::bind(&McachedHttpService::ThreadWorker, this));
    checker_thread_ = std::make_shared<Thread>(std::bind(&McachedHttpService::CheckerThreadWorker, this));
    return true;
}

bool moxie::McachedHttpService::Start() {
    return thread_->Start() && checker_thread_->Start();
}

void moxie::McachedHttpService::SlotKeeper(HttpRequest& request, HttpResponse& response) {
    ClientRequestArgs args;
    if (!ParseClientRequestArgs(request, args)) {
        std::cout << "PostProcess[0]" << std::endl;
        Http4xxResponse(response, "400", "Bad Request", request.GetVersion());
        return;
    }

    std::cout << "PostProcess[1]" << std::endl;
    switch (args.cmd_type) {
        case SlotKeeperCmd::CmdAddSlotToGroup:
            ProcessCmdAddSlot(args, request, response);
            break;
        case SlotKeeperCmd::CmdDelSlotFromGroup:
            ProcessCmdDelSlot(args, request, response);
            break;
        case SlotKeeperCmd::CmdMoveSlotStart:
            ProcessCmdMoveSlot(args, request, response);
            break;
        default:
            std::cout << "PostProcess[2]" << std::endl;
            Http4xxResponse(response, "400", "Cmd Not Found", request.GetVersion());
            return;
    }
    std::cout << "PostProcess[3]" << std::endl;
    return;
}

void moxie::McachedHttpService::PostProcess(HttpRequest& request, HttpResponse& response) {
    std::cout << "PostProcess:" << request.GetPath() << std::endl;
    if (request.GetPath() == this->managerServiceConf_.slotkeeper) {
        this->SlotKeeper(request, response);
    } else {
        std::cout << "Path handler not found!" << std::endl;
    }
}

bool moxie::McachedHttpService::ParseClientRequestArgs(HttpRequest& request, ClientRequestArgs& args) {
    Json::Reader reader;  
    Json::Value root;
    if (request.GetBodyLength() == 0) {
        return false;
    } 

    std::string body = std::string(request.GetBodyData(), request.GetBodyLength());
    if (!reader.parse(body, root)) {
        return false;
    }

    if (!(root.isMember("cmd_type") && root["cmd_type"].isInt())) {
        return false;
    }

    if (!(root.isMember("slot_id") && root["slot_id"].isInt())) {
        return false;
    }

    if (!(root.isMember("source_id") && root["source_id"].isInt())) {
        return false;
    }

    if (!(root.isMember("dest_id") && root["dest_id"].isInt())) {
        return false;
    }

    args.cmd_type = root["cmd_type"].asInt();
    args.slot_id = root["slot_id"].asInt();
    args.source_id = root["source_id"].asInt();
    args.dest_id = root["dest_id"].asInt();
    return true;
}

void moxie::McachedHttpService::ProcessCmdAddSlot(const ClientRequestArgs& args, HttpRequest& request, HttpResponse& response) {
    std::cout << "ProcessCmdAddSlot" << std::endl;
    assert(args.cmd_type == SlotKeeperCmd::CmdAddSlotToGroup);
    if (args.slot_id < SlotRange::McachedSlotsStart || args.slot_id >= SlotRange::McachedSlotsEnd) {
        Http4xxResponse(response, "400", "Bad Request!", request.GetVersion());
        return;
    }

    if (ServiceMeta::Instance()->CacheId() != args.source_id 
        ||!ServiceMeta::Instance()->CacheIdActivated()) {
        std::cout << "ProcessCmdAddSlot 404" << std::endl;
        Http4xxResponse(response, "400", "Bad Request!", request.GetVersion());
        return;
    }

    Json::Value root;
    Json::FastWriter writer;
    root["cmd_type"] = SlotKeeperCmd::CmdAddSlotToGroup;
    root["source_id"] = ServiceMeta::Instance()->CacheId();
    root["dest_id"] = 0;
    root["slot_id"] = args.slot_id;

    std::string body = writer.write(root);
    std::string post_res = "";
    struct CurlExt ext;

    auto cur_url = this->cur_manager_url_ + this->managerServiceConf_.slotkeeper;
    if (PostByCurl(cur_url, body, post_res, ext) != CURLE_OK) {
        for (size_t index = 0; index < this->managerServiceConf_.urls.size(); ++index) {
            auto cur_url_retry = this->managerServiceConf_.urls[index] + this->managerServiceConf_.slotkeeper;
            if (cur_url == cur_url_retry) {
                continue;
            }
            if (PostByCurl(cur_url_retry, body, post_res, ext) == CURLE_OK) {
                this->cur_manager_url_ = this->managerServiceConf_.urls[index];
                break;
            }
        }
    }

    if (ext.status_code == 200) {
        response.SetStatus("OK");
    } else {
        response.SetStatus("Error");
    }
    response.SetScode(std::to_string(ext.status_code));
    response.SetVersion(request.GetVersion());

    std::string content = post_res;
    response.AppendBody(content.c_str(), content.size());

    response.PutHeaderItem("Content-Type", "text/json");
    response.PutHeaderItem("Content-Length", std::to_string(content.size()));
}

void moxie::McachedHttpService::ProcessCmdDelSlot(const ClientRequestArgs& args, HttpRequest& request, HttpResponse& response) {
    assert(args.cmd_type == SlotKeeperCmd::CmdDelSlotFromGroup);
    if (args.slot_id < SlotRange::McachedSlotsStart || args.slot_id >= SlotRange::McachedSlotsEnd) {
        Http4xxResponse(response, "400", "Bad Request!", request.GetVersion());
        return;
    }

    if (ServiceMeta::Instance()->CacheId() != args.source_id 
        ||!ServiceMeta::Instance()->CacheIdActivated()) {
        std::cout << "ProcessCmdDelSlot 404" << std::endl;
        Http4xxResponse(response, "400", "Bad Request!", request.GetVersion());
        return;
    }

    Json::Value root;
    Json::FastWriter writer;
    root["cmd_type"] = SlotKeeperCmd::CmdDelSlotFromGroup;
    root["source_id"] = ServiceMeta::Instance()->CacheId();
    root["dest_id"] = 0;
    root["slot_id"] = args.slot_id;

    std::string body = writer.write(root);
    std::string post_res = "";
    struct CurlExt ext;

    auto cur_url = this->cur_manager_url_ + this->managerServiceConf_.slotkeeper;
    if (PostByCurl(cur_url, body, post_res, ext) != CURLE_OK) {
        for (size_t index = 0; index < this->managerServiceConf_.urls.size(); ++index) {
            auto cur_url_retry = this->managerServiceConf_.urls[index] + this->managerServiceConf_.slotkeeper;
            if (cur_url == cur_url_retry) {
                continue;
            }
            if (PostByCurl(cur_url_retry, body, post_res, ext) == CURLE_OK) {
                this->cur_manager_url_ = this->managerServiceConf_.urls[index];
                break;
            }
        }
    }

    if (ext.status_code == 200) {
        response.SetStatus("OK");
        // TODO:sxfworks
    } else {
        response.SetStatus("Error");
    }

    response.SetScode(std::to_string(ext.status_code));
    response.SetVersion(request.GetVersion());

    std::string content = post_res;
    response.AppendBody(content.c_str(), content.size());

    response.PutHeaderItem("Content-Type", "text/json");
    response.PutHeaderItem("Content-Length", std::to_string(content.size()));
}

void moxie::McachedHttpService::ProcessCmdMoveSlot(const ClientRequestArgs& args, HttpRequest& request, HttpResponse& response) {
    assert(args.cmd_type == SlotKeeperCmd::CmdMoveSlotStart); 
   
    if (args.slot_id < SlotRange::McachedSlotsStart || args.slot_id >= SlotRange::McachedSlotsEnd) {
        Http4xxResponse(response, "400", "Bad Request!", request.GetVersion());
        return;
    }

    if (ServiceMeta::Instance()->CacheId() != args.source_id 
        ||!ServiceMeta::Instance()->CacheIdActivated()) {
        Http4xxResponse(response, "400", "Bad Request!", request.GetVersion());
        return;
    }

    Json::Value root;
    Json::FastWriter writer;
    if (args.source_id != ServiceMeta::Instance()->CacheId()) {
        Http4xxResponse(response, "400", "Bad Request!", request.GetVersion());
        return;
    }
    root["cmd_type"] = SlotKeeperCmd::CmdMoveSlotStart;
    root["source_id"] = ServiceMeta::Instance()->CacheId();
    root["dest_id"] = args.dest_id;
    root["slot_id"] = args.slot_id;

    std::string body = writer.write(root);
    std::string post_res = "";
    struct CurlExt ext;

    auto cur_url = this->cur_manager_url_ + this->managerServiceConf_.slotkeeper;
    if (PostByCurl(cur_url, body, post_res, ext) != CURLE_OK) {
        for (size_t index = 0; index < this->managerServiceConf_.urls.size(); ++index) {
            auto cur_url_retry = this->managerServiceConf_.urls[index] + this->managerServiceConf_.slotkeeper;
            if (cur_url == cur_url_retry) {
                continue;
            }
            if (PostByCurl(cur_url_retry, body, post_res, ext) == CURLE_OK) {
                this->cur_manager_url_ = this->managerServiceConf_.urls[index];
                break;
            }
        }
    }

    if (ext.status_code == 200) {
        response.SetStatus("OK");
        // TODO:sxfworks
    } else {
        response.SetStatus("Error");
    }

    response.SetScode(std::to_string(ext.status_code));
    response.SetVersion(request.GetVersion());

    std::string content = post_res;
    response.AppendBody(content.c_str(), content.size());

    response.PutHeaderItem("Content-Type", "text/json");
    response.PutHeaderItem("Content-Length", std::to_string(content.size()));
}

void moxie::McachedHttpService::Http4xxResponse(HttpResponse& response,
                                                const std::string& code,
                                                const std::string& status,
                                                const std::string& version) {
    response.SetScode(code);
    response.SetStatus(status);
    response.SetVersion(version);
}

//回调函数 得到响应内容
int write_data(char* buffer, size_t size, size_t nmemb, void* userp) {
    std::string *str = dynamic_cast<std::string *>((std::string *)userp);
    str->append((char *)buffer, size * nmemb);
    return nmemb;
}

int moxie::McachedHttpService::GetByCurl(const std::string& url, 
                                        std::string& response,
                                        struct CurlExt& ext) {
    CURL *curl = curl_easy_init();
    if (!curl) {
        return -1;
    } 

    curl_easy_setopt(curl, CURLOPT_URL, (char *)url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);
    CURLcode ret = curl_easy_perform(curl);

    if (ret == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &ext.status_code);
        char *rurl = nullptr;
        curl_easy_getinfo(curl, CURLINFO_REDIRECT_URL, &rurl);
        if (rurl) {
            ext.redirect_url = rurl;
        }
    }

    curl_easy_cleanup(curl); 
    return ret; 
}

int moxie::McachedHttpService::PostByCurl(const std::string& url, 
                                          std::string &body, 
                                          std::string &response, 
                                          struct CurlExt& ext) {
    CURL *curl = curl_easy_init();
    if (!curl) {
        return -1;
    }

    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, body.size());
    curl_easy_setopt(curl, CURLOPT_URL, (char *)url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&response);
    CURLcode ret = curl_easy_perform(curl);
    
    if (ret == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &ext.status_code);
        char *rurl = nullptr;
        curl_easy_getinfo(curl, CURLINFO_REDIRECT_URL, &rurl);
        if (rurl) {
            ext.redirect_url = rurl;
        }
    }

    curl_easy_cleanup(curl);
    return ret;
}

void moxie::McachedHttpService::CreateCachedId() {
    assert(ServiceMeta::Instance()->CacheId() == 0);
    Json::Value root;
    Json::FastWriter writer;
    root["cmd_type"] = GroupKeeperCmd::CmdCreateCacheGroup;
    root["group_id"] = 0;
    root["ext"] = "";

    std::string body = writer.write(root);
    std::string post_res = "";
    struct CurlExt ext;

    auto cur_url = this->cur_manager_url_ + this->managerServiceConf_.groupleeper;
    if (PostByCurl(cur_url, body, post_res, ext) != CURLE_OK) {
        for (size_t index = 0; index < this->managerServiceConf_.urls.size(); ++index) {
            auto cur_url_retry = this->managerServiceConf_.urls[index] + this->managerServiceConf_.groupleeper;
            if (cur_url == cur_url_retry) {
                continue;
            }
            if (PostByCurl(cur_url_retry, body, post_res, ext) == CURLE_OK) {
                this->cur_manager_url_ = this->managerServiceConf_.urls[index];
                break;
            }
        }
    }

    std::cout << "[CreateCachedId]post_res:" << post_res << std::endl;
    if (ext.status_code == 200) {
        Json::Reader reader;  
        Json::Value body_root;
        if (reader.parse(post_res, body_root) 
            && body_root.isMember("ext") 
            && body_root["ext"].isString()
            && body_root.isMember("succeed")
            && body_root["succeed"].isBool()
            && body_root["succeed"].asBool()) {
            std::string ext = body_root["ext"].asString();
            int64_t group_id = std::atoll(ext.c_str());
            if (group_id <= 0) {
                std::cout << "Create group id failed!" << std::endl;
            } else {
                ServiceMeta::Instance()->CacheId(group_id);
            }
        } else {
            std::cout << "Create group id failed!" << std::endl;
        }
    } else {
        std::cout << "Create group id failed!" << std::endl;
    }
}

void moxie::McachedHttpService::ActivatedCachedId() {
    assert(ServiceMeta::Instance()->CacheId() != 0);
    assert(!ServiceMeta::Instance()->CacheIdActivated());
    Json::Value root;
    Json::FastWriter writer;
    root["cmd_type"] = GroupKeeperCmd::CmdActivateGroup;
    root["group_id"] = ServiceMeta::Instance()->CacheId();
    root["ext"] = "";

    std::string body = writer.write(root);
    std::string post_res = "";
    struct CurlExt ext;

    auto cur_url = this->cur_manager_url_ + this->managerServiceConf_.groupleeper;
    if (PostByCurl(cur_url, body, post_res, ext) != CURLE_OK) {
        for (size_t index = 0; index < this->managerServiceConf_.urls.size(); ++index) {
            auto cur_url_retry = this->managerServiceConf_.urls[index] + this->managerServiceConf_.groupleeper;
            if (cur_url == cur_url_retry) {
                continue;
            }
            if (PostByCurl(cur_url_retry, body, post_res, ext) == CURLE_OK) {
                this->cur_manager_url_ = this->managerServiceConf_.urls[index];
                break;
            }
        }
    }
    std::cout << "[ActivatedCachedId]post_res:" << post_res << std::endl;
    if (ext.status_code == 200) {
        Json::Reader reader;  
        Json::Value body_root;
        if (reader.parse(post_res, body_root) 
            && body_root.isMember("ext") 
            && body_root["ext"].isString()
            && body_root.isMember("succeed")
            && body_root["succeed"].isBool()
            && body_root["succeed"].asBool()) {
            ServiceMeta::Instance()->CacheIdActivated(true);
        } else {
            std::cout << "Activated group id failed!" << std::endl;
        }
    } else {
        std::cout << "Activated group id failed!" << std::endl;
    }
}

void moxie::McachedHttpService::ThreadWorker() {
    assert(loop_);
    if (!loop_->Register(this->event_, this->server_)) {
        LOGGER_ERROR("Loop Register Error");
        delete loop_;
        return;
    }

    loop_->Loop();
}

void moxie::McachedHttpService::CheckerThreadWorker() {
    assert(this->checker_loop_ && this->checker_timer_);
    if (!checker_loop_->RegisterTimer(this->checker_timer_)) {
        LOGGER_ERROR("Loop Register Error");
        delete checker_loop_;
        return;
    }

    if (!checker_loop_->RegisterTimer(this->keepalive_timer_)) {
        LOGGER_ERROR("Loop Register Error");
        delete checker_loop_;
        return;
    }

    checker_loop_->Loop();
}

void moxie::McachedHttpService::KeepAliveTimerWorker(moxie::Timer *timer, moxie::EventLoop *loop) {
    this->KeepAlive();
}

void moxie::McachedHttpService::KeepAlive() {
    if (ServiceMeta::Instance()->CacheId() == 0
        || !ServiceMeta::Instance()->CacheIdActivated()) {
        return;
    }
    uint64_t gid = ServiceMeta::Instance()->CacheId();
    Json::Value root;
    Json::FastWriter writer;
    root["cmd_type"] = CmdKeepalive::GroupKeepAlive;
    root["is_master"] = true;
    root["hosts"] = this->mcachedConf_.ip + ":" + std::to_string(this->mcachedConf_.port);
    root["source_id"] = gid;
    root["server_name"] = this->mcachedConf_.serverName;

    std::string body = writer.write(root);
    std::string post_res = "";
    struct CurlExt ext;

    auto cur_url = this->cur_manager_url_ + this->managerServiceConf_.keepalive;
    std::cout << "cur_url:" << cur_url << std::endl;
    if (PostByCurl(cur_url, body, post_res, ext) != CURLE_OK) {
        for (size_t index = 0; index < this->managerServiceConf_.urls.size(); ++index) {
            auto cur_url_retry = this->managerServiceConf_.urls[index] + this->managerServiceConf_.keepalive;
            if (cur_url == cur_url_retry) {
                continue;
            }
            if (PostByCurl(cur_url_retry, body, post_res, ext) == CURLE_OK) {
                this->cur_manager_url_ = this->managerServiceConf_.urls[index];
                break;
            }
        }
    }

    std::cout << "[KeepAlive]post_res:" << post_res << std::endl;
    if (ext.status_code == 200) {
        Json::Reader reader;  
        Json::Value body_root;
        if (reader.parse(post_res, body_root) 
            && body_root.isMember("succeed")
            && body_root["succeed"].isBool()
            && body_root["succeed"].asBool()) {
        } else {
            std::cout << "KeepAlive[1] group_id[" << gid << "] failed!" << std::endl;
        }
    } else {
        std::cout << "KeepAlive[2] group_id[" << gid << "] failed!" << std::endl;
    }
}

void moxie::McachedHttpService::CheckerTimerWorker(moxie::Timer *timer, moxie::EventLoop *loop) {
    if (ServiceMeta::Instance()->CacheId() == 0) {
        this->CreateCachedId();
    } else {
        if (!ServiceMeta::Instance()->CacheIdActivated()) {
            this->ActivatedCachedId();
        }
    }

    //must be after do sth done
    timer->Reset(moxie::AddTime(Timestamp::Now(), 1));
}
