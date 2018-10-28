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

bool moxie::McachedHttpService::Init(const HttpServiceConf& conf) {
    conf_ = conf;
    int server = socket(AF_INET, SOCK_STREAM, 0);
    if (server < 0) {
        LOGGER_ERROR("socket error : " << strerror(errno));
        return false;
    }

    curl_global_init(CURL_GLOBAL_ALL);

    Socket::SetExecClose(server);
    Socket::SetNoBlocking(server);
    Socket::SetReusePort(server);

    addr_ = NetAddress(AF_INET, conf_.port, conf_.ip.c_str());

    if (!(Socket::Bind(server, addr_)
          && Socket::Listen(server, 128))) {
        return false;
    }

    this->manager_url_list_ = moxie::utils::StringSplit(conf_.manager_server_list, ";");
    if (this->manager_url_list_.size() == 0) {
        return false;
    }
    this->cur_manager_url_ = this->manager_url_list_[0];

    event_ = std::make_shared<PollerEvent>(server, moxie::kReadEvent);
    server_->RegisterMethodCallback("POST", std::bind(&McachedHttpService::PostProcess, this, std::placeholders::_1, std::placeholders::_2));
    server_->RegisterMethodCallback("post", std::bind(&McachedHttpService::PostProcess, this, std::placeholders::_1, std::placeholders::_2));

    server_->RegisterMethodCallback("GET", std::bind(&McachedHttpService::GetProcess, this, std::placeholders::_1, std::placeholders::_2));
    server_->RegisterMethodCallback("get", std::bind(&McachedHttpService::GetProcess, this, std::placeholders::_1, std::placeholders::_2));

    checker_timer_ = new Timer(std::bind(&McachedHttpService::CheckerTimerWorker, this, std::placeholders::_1, std::placeholders::_2), moxie::AddTime(Timestamp::Now(), 0.5));

    thread_ = std::make_shared<Thread>(std::bind(&McachedHttpService::ThreadWorker, this));
    checker_thread_ = std::make_shared<Thread>(std::bind(&McachedHttpService::CheckerThreadWorker, this));
    return true;
}

bool moxie::McachedHttpService::Start() {
    return thread_->Start() && checker_thread_->Start();
}

void moxie::McachedHttpService::PostProcess(HttpRequest& request, HttpResponse& response) {
    std::cout << "PostProcess" << std::endl;
    ClientRequestArgs args;
    if (!ParseClientRequestArgs(request, args)) {
        std::cout << "PostProcess[0]" << std::endl;
        Http4xxResponse(response, "400", "Bad Request", request.GetVersion());
        return;
    }
    std::cout << "PostProcess[1]" << std::endl;
    switch (args.cmd_type) {
        case CmdFromClient::CmdAddSlot:
            ProcessCmdAddSlot(args, request, response);
            break;
        case CmdFromClient::CmdDelSlot:
            ProcessCmdDelSlot(args, request, response);
            break;
        case CmdFromClient::CmdMoveSlot:
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

    if (!(root.isMember("group_id") && root["group_id"].isInt())) {
        return false;
    }

    args.cmd_type = root["cmd_type"].asInt();
    args.slot_id = root["slot_id"].asInt();
    args.group_id = root["group_id"].asInt();
    return true;
}

void moxie::McachedHttpService::ProcessCmdAddSlot(const ClientRequestArgs& args, HttpRequest& request, HttpResponse& response) {
    std::cout << "ProcessCmdAddSlot" << std::endl;
    assert(args.cmd_type == CmdFromClient::CmdAddSlot);
    if (args.slot_id < SlotRange::McachedSlotsStart || args.slot_id >= SlotRange::McachedSlotsEnd) {
        Http4xxResponse(response, "400", "Bad Request!", request.GetVersion());
        return;
    }
    Json::Value root;
    Json::FastWriter writer;
    root["cmd_type"] = CmdReverseType::CmdAddSlotToGroup;
    root["source_id"] = ServiceMeta::Instance()->CacheId();
    root["dest_id"] = 0;
    root["slot_id"] = args.slot_id;

    std::string body = writer.write(root);
    std::string post_res = "";
    struct CurlExt ext;

    if (PostByCurl(this->cur_manager_url_, body, post_res, ext) != CURLE_OK) {
        for (size_t index = 0; index < this->manager_url_list_.size(); ++index) {
            if (this->manager_url_list_[index] == this->cur_manager_url_) {
                continue;
            }
            if (PostByCurl(this->manager_url_list_[index], body, post_res, ext) == CURLE_OK) {
                this->cur_manager_url_ = this->manager_url_list_[index];
                break;
            }
        }
    }

    if (300 <= ext.status_code && ext.status_code < 400) {
        this->cur_manager_url_ = ext.redirect_url;
        PostByCurl(this->cur_manager_url_, body, post_res, ext);
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

void moxie::McachedHttpService::ProcessCmdDelSlot(const ClientRequestArgs& args, HttpRequest& request, HttpResponse& response) {
    assert(args.cmd_type == CmdFromClient::CmdDelSlot);
    if (args.slot_id < SlotRange::McachedSlotsStart || args.slot_id >= SlotRange::McachedSlotsEnd) {
        Http4xxResponse(response, "400", "Bad Request!", request.GetVersion());
        return;
    }

    Json::Value root;
    Json::FastWriter writer;
    root["cmd_type"] = CmdReverseType::CmdDelSlotFromGroup;
    root["source_id"] = ServiceMeta::Instance()->CacheId();
    root["dest_id"] = 0;
    root["slot_id"] = args.slot_id;

    std::string body = writer.write(root);
    std::string post_res = "";
    struct CurlExt ext;

    if (PostByCurl(this->cur_manager_url_, body, post_res, ext) != CURLE_OK) {
        for (size_t index = 0; index < this->manager_url_list_.size(); ++index) {
            if (this->manager_url_list_[index] == this->cur_manager_url_) {
                continue;
            }
            if (PostByCurl(this->manager_url_list_[index], body, post_res, ext) == CURLE_OK) {
                this->cur_manager_url_ = this->manager_url_list_[index];
                break;
            }
        }
    }

    if (300 <= ext.status_code && ext.status_code < 400) {
        this->cur_manager_url_ = ext.redirect_url;
        PostByCurl(this->cur_manager_url_, body, post_res, ext);
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
    assert(args.cmd_type == CmdFromClient::CmdMoveSlot);
    if (args.slot_id < SlotRange::McachedSlotsStart || args.slot_id >= SlotRange::McachedSlotsEnd) {
        Http4xxResponse(response, "400", "Bad Request!", request.GetVersion());
        return;
    }
}

void moxie::McachedHttpService::GetProcess(HttpRequest& request, HttpResponse& response) {
    response.SetScode("200");
    response.SetStatus("OK");
    response.SetVersion(request.GetVersion());

    std::string content = "<html><body> " + std::string("In Get Method!") + " </body></html>";
    response.AppendBody(content.c_str(), content.size());

    response.PutHeaderItem("Content-Type", "text/html");
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
    root["cmd_type"] = CmdReverseType::CmdCreateCacheGroup;
    root["source_id"] = 0;
    root["dest_id"] = 0;
    root["slot_id"] = 0;

    std::string body = writer.write(root);
    std::string post_res = "";
    struct CurlExt ext;

    if (PostByCurl(this->cur_manager_url_, body, post_res, ext) != CURLE_OK) {
        for (size_t index = 0; index < this->manager_url_list_.size(); ++index) {
            if (this->manager_url_list_[index] == this->cur_manager_url_) {
                continue;
            }
            if (PostByCurl(this->manager_url_list_[index], body, post_res, ext) == CURLE_OK) {
                this->cur_manager_url_ = this->manager_url_list_[index];
                break;
            }
        }
    }

    if (300 <= ext.status_code && ext.status_code < 400) {
        this->cur_manager_url_ = ext.redirect_url;
        PostByCurl(this->cur_manager_url_, body, post_res, ext);
    }
    std::cout << "post_res:" << post_res << std::endl;
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
    root["cmd_type"] = CmdReverseType::CmdActivateGroup;
    root["source_id"] = ServiceMeta::Instance()->CacheId();
    root["dest_id"] = 0;
    root["slot_id"] = 0;

    std::string body = writer.write(root);
    std::string post_res = "";
    struct CurlExt ext;

    if (PostByCurl(this->cur_manager_url_, body, post_res, ext) != CURLE_OK) {
        for (size_t index = 0; index < this->manager_url_list_.size(); ++index) {
            if (this->manager_url_list_[index] == this->cur_manager_url_) {
                continue;
            }
            if (PostByCurl(this->manager_url_list_[index], body, post_res, ext) == CURLE_OK) {
                this->cur_manager_url_ = this->manager_url_list_[index];
                break;
            }
        }
    }

    if (300 <= ext.status_code && ext.status_code < 400) {
        this->cur_manager_url_ = ext.redirect_url;
        PostByCurl(this->cur_manager_url_, body, post_res, ext);
    }

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

    checker_loop_->Loop();
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
