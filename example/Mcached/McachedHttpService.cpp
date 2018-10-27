#include <functional>

#include <json/json.h>

#include <McachedHttpService.h>
#include <Socket.h>

moxie::McachedHttpService::McachedHttpService()
    : server_(std::make_shared<HttpServer>()) {
}

bool moxie::McachedHttpService::Init(const HttpServiceConf& conf) {
    conf_ = conf;
    int server = socket(AF_INET, SOCK_STREAM, 0);
    if (server < 0) {
        LOGGER_ERROR("socket error : " << strerror(errno));
        return false;
    }

    Socket::SetExecClose(server);
    Socket::SetNoBlocking(server);
    Socket::SetReusePort(server);

    addr_ = NetAddress(AF_INET, conf_.port, conf_.ip.c_str());

    if (!(Socket::Bind(server, addr_)
          && Socket::Listen(server, 128))) {
        return false;
    }

    event_ = std::make_shared<PollerEvent>(server, moxie::kReadEvent);
    server_->RegisterMethodCallback("POST", std::bind(&McachedHttpService::PostProcess, this, std::placeholders::_1, std::placeholders::_2));
    server_->RegisterMethodCallback("post", std::bind(&McachedHttpService::PostProcess, this, std::placeholders::_1, std::placeholders::_2));

    server_->RegisterMethodCallback("GET", std::bind(&McachedHttpService::GetProcess, this, std::placeholders::_1, std::placeholders::_2));
    server_->RegisterMethodCallback("get", std::bind(&McachedHttpService::GetProcess, this, std::placeholders::_1, std::placeholders::_2));


    thread_ = std::make_shared<Thread>(std::bind(&McachedHttpService::ThreadWorker, this));
    return true;
}

bool moxie::McachedHttpService::Start() {
    return thread_->Start();
}

void moxie::McachedHttpService::PostProcess(HttpRequest& request, HttpResponse& response) {
    Json::Reader reader;  
    Json::Value root;
    if (request.GetBodyLength() == 0) {
        // error
        std::cout << "Get Body data failed!" << std::endl;
        Http4xxResponse("400", "Bad Request", request.GetVersion());
        return;
    } 
    std::string body = std::string(request.GetBodyData(), request.GetBodyLength());
    if (!reader.parse(body, root)) {
        // error
        std::cout << "The format of body is not json!" << std::endl;
        Http4xxResponse("400", "Bad Request", request.GetVersion());
        return;
    }

    if (!(root.isMember("cmd_type") && root["cmd_type"].isInt())) {
        std::cout << "Cmd type can not be parse." << std::endl;
        Http4xxResponse("400", "Bad Request", request.GetVersion());
        return;
    }

    //int cmd_type = root["cmd_type"].asInt();

    Http4xxResponse("200", "OK", request.GetVersion());
    Json::FastWriter writer;  
    std::string strWrite = writer.write(root);
    std::string content = "<html><body> " + strWrite + " </body></html>";
    response.AppendBody(content.c_str(), content.size());
    response.PutHeaderItem("Content-Type", "text/html");
    response.PutHeaderItem("Content-Length", std::to_string(content.size()));
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

int moxie::McachedHttpService::GetByCurl(const string& url, string* response) {
    CURL *curl;
    CURLcode ret;
    struct curl_slist *headers = NULL;
    headers = curl_slist_append(headers, "Accept: Agent-007");
    curl = curl_easy_init();
    // 初始化
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        //改协议头
        curl_easy_setopt(curl, CURLOPT_URL, (char *)url.c_str());
        // 指定url
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)response);
        int status_code = curl_easy_perform(curl);
        // 执行
        ret = curl_easy_perform(curl);
        //执行请求
        if(ret == 0) {
            curl_slist_free_all(headers);
            curl_easy_cleanup(curl); 
            return status_code;
        } else {
            return ret;
        } 
    } else {
        return -1; 
    }
}

int moxie::McachedHttpService::PostByCurl(const string& url, string &body,  string* response) {
    CURL *curl;
    CURLcode ret;
    curl = curl_easy_init();
    long response_code = 0;
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
        //指定post内容
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, body.size());
        //指定post长度
        curl_easy_setopt(curl, CURLOPT_HEADER, 0);
        //设置协议头
        // curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        curl_easy_setopt(curl, CURLOPT_URL, (char *)url.c_str());
        //指定url
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
        //绑定相应
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)response);
        //绑定响应内容的地址
        ret = curl_easy_perform(curl);
        //执行请求
        if(ret == 0) {
            curl_easy_cleanup(curl);
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
            //获取http 状态码 
            return 0;
        } else {
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
            //获取http 状态码
            return ret; 
        } 
    } else {
        return -1;
    }
}

void moxie::McachedHttpService::ThreadWorker() {
    loop_ = new EventLoop();

    if (!loop_->Register(this->event_, this->server_)) {
        LOGGER_ERROR("Loop Register Error");
        delete loop_;
        return;
    }

    loop_->Loop();
    delete loop_;
}