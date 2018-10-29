#include <iostream>
#include <vector>
#include <string>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <atomic>

#include <EventLoop.h>
#include <string.h>
#include <Log.h>
#include <Socket.h>
#include <NetAddress.h>
#include <EventLoopThread.h>
#include <PollerEvent.h>
#include <McachedServer.h>
#include <McachedHttpService.h>

using namespace moxie;

const std::string httpservice_work_path = "/mcached/service";

struct WorkContext {
    short port;
    std::string ip;
};

void RegisterMcachedWorkerTask(EventLoop *loop, WorkContext *ctx) {
    if (!ctx) {
        LOGGER_ERROR("Ctx is nulptr!");
        return;
    }

    int server = socket(AF_INET, SOCK_STREAM, 0);
    if (server < 0) {
        LOGGER_ERROR("socket error : " << strerror(errno));
        return;
    }

    Socket::SetExecClose(server);
    Socket::SetNoBlocking(server);
    Socket::SetReusePort(server);

    NetAddress addr(AF_INET, ctx->port, ctx->ip.c_str());

    if (!(Socket::Bind(server, addr)
          && Socket::Listen(server, 128))) {
        return;
    }

    // 注册监听套接字的处理类
    std::shared_ptr<PollerEvent> event = std::make_shared<PollerEvent>(server, moxie::kReadEvent);
    if (!loop->Register(event, std::make_shared<McachedServer>())) {
        LOGGER_ERROR("Loop Register Error");
        return;
    }
}

int main(int argc, char **argv) {
    if (argc < 4) {
        std::cout << "Usage:mcached 127.0.0.1 6379 threads" << std::endl; 
        return -1;
    }

    WorkContext *ctx = new (std::nothrow) WorkContext;
    if (ctx == nullptr) {
        LOGGER_ERROR("New WorkContext failed!");
        return -1;
    }

    ctx->port = std::atoi(argv[2]);
    ctx->ip = argv[1];
    int threads = std::atoi(argv[3]);
    if (threads < 1) {
        LOGGER_ERROR("Thread must larger than 1!");
        return -1;
    }
    
    std::vector<std::shared_ptr<EventLoopThread>> thds; thds.reserve(threads);
    for (int i = 0; i < threads; ++i) {
        EventLoop *loop = new EventLoop;
        if (loop && ctx) {
            auto td = std::make_shared<EventLoopThread>();
            td->Start();
            td->PushTask(std::bind(RegisterMcachedWorkerTask, std::placeholders::_1, ctx));
            thds.push_back(td);
        }
    }

    HttpServiceConf conf;
    conf.ip = "127.0.0.1";
    conf.port = 13579;
    conf.mcached_hosts = argv[1] + std::string(":") + argv[2];
    conf.work_path = httpservice_work_path;
    conf.manager_server_list = "http://127.0.0.1:8898/Mcached/GroupRevise/";
    conf.keepalive_server_list = "http://127.0.0.1:8898/Mcached/EndPoints/";

    McachedHttpService service;
    if (service.Init(conf)) {
        service.Start();
    } else {
        LOGGER_ERROR("Init Httpservice failed!");
        return -1;
    }

    for (size_t i = 0; i < thds.size(); ++i) {
        thds[i]->Join();
    }

    delete ctx;
    return 0;
}
