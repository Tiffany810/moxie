#ifndef MOXIE_SERVICEMETACHECKER_H
#define MOXIE_SERVICEMETACHECKER_H
#include <memory>
#include <string>

#include <PollerEvent.h>
#include <EventLoop.h>
#include <Thread.h>

namespace moxie {

class ServiceMetaChecker : public Wakeuper {
public:
    void Process(const std::shared_ptr<PollerEvent>& event, EventLoop *loop);
private:
    void ThreadWorker();
private:
    EventLoop *loop_;
    std::shared_ptr<Thread> thread_;
};

}

#endif 