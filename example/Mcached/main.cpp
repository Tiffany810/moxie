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
#include <mraft/floyd/src/floyd_impl.h>
#include <IdGenerator.h>

using namespace moxie;
using namespace floyd;

std::shared_ptr<floyd::FloydImpl> floyd_raft;
IdGenerator *igen;

const std::string httpservice_work_path = "/mcached/service";

struct WorkContext {
    short port;
    std::string ip;
};

void RegisterMcachedWorkerTask(EventLoop *loop, WorkContext *ctx, std::shared_ptr<EventLoopThread> et) {
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
    if (!loop->Register(event, std::make_shared<McachedServer>(et))) {
        LOGGER_ERROR("Loop Register Error");
        return;
    }
}

void RegisterFloydImpl(moxie::EventLoop* loop, std::shared_ptr<floyd::FloydImpl> impl) {
    std::cout << "RegisterFloydImpl Process[" << impl->GetWakeUpFd() << "] ok!" << std::endl;
    std::shared_ptr<moxie::PollerEvent> event(new moxie::PollerEvent(impl->GetWakeUpFd(), moxie::kReadEvent));
    loop->Register(event, impl);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        std::cout << "Usage:mcached conf" << std::endl; 
        return -1;
    }

    moxie::Conf conf;
    if (conf.ParseConf(argv[1]) == false) {
        std::cout << "Parse mcached conf failed!" << std::endl;
        return -1;
    }

    auto raftConf = conf.GetRaftConf();
    Options op(raftConf.cluster, raftConf.ip, raftConf.port, raftConf.data);
    op.Dump();

    slash::Status s;
    floyd_raft = std::shared_ptr<floyd::FloydImpl>(new floyd::FloydImpl(op));
    s = floyd_raft->Init();
    if (!s.ok()) {
        std::cout << s.ToString().c_str() << std::endl;
        return -1;
    }

    floyd_raft->SetApplyNotify(McachedServer::ApplyRaftTask);

    auto raft_thread = std::make_shared<EventLoopThread>();
    raft_thread->Start();
    raft_thread->PushTask(std::bind(RegisterFloydImpl, std::placeholders::_1, floyd_raft));
    std::cout << "Floyd raft init " << s.ToString().c_str() << "!" << std::endl;

    WorkContext *ctx = new (std::nothrow) WorkContext;
    if (ctx == nullptr) {
        LOGGER_ERROR("New WorkContext failed!");
        return -1;
    }

    auto mcached_conf = conf.GetMcachedConf();
    auto service_conf = conf.GetServiceConf();
    auto mgr_conf = conf.GetManagerServiceConf();

    ctx->port = mcached_conf.port;
    ctx->ip = mcached_conf.ip;
    int threads = mcached_conf.threads;
    if (threads < 1) {
        LOGGER_ERROR("Thread must larger than 1!");
        return -1;
    }
    igen = new IdGenerator(mcached_conf.id, Timestamp::NanoSeconds());

    std::vector<std::shared_ptr<EventLoopThread>> thds; thds.reserve(threads);
    for (int i = 0; i < threads; ++i) {
        EventLoop *loop = new EventLoop;
        if (loop && ctx) {
            auto td = std::make_shared<EventLoopThread>();
            td->Start();
            td->PushTask(std::bind(RegisterMcachedWorkerTask, std::placeholders::_1, ctx, td));
            thds.push_back(td);
        }
    }

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
