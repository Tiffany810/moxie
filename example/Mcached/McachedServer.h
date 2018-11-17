#ifndef MOXIE_MECACHEDSERVER_H
#define MOXIE_MECACHEDSERVER_H
#include <memory>
#include <unistd.h>
#include <unordered_map>

#include <EventLoop.h>
#include <Mutex.h>
#include <PollerEvent.h>
#include <ListenHadler.h>
#include <NetAddress.h>
#include <McachedRequestHandler.h>
#include <IdGenerator.h>
#include <mcached/Mcached.h>

#include <mraft/floyd/src/raft_task.h>

namespace moxie {

class McachedServer : public ListenHadler {
public:
    virtual void AfterAcceptSuccess(const std::shared_ptr<PollerEvent>& client, EventLoop *loop, const std::shared_ptr<moxie::NetAddress>& cad) override;
    static int ApplyMcached(std::vector<std::string>& args, std::string& response);
    static bool ApplyRaftTask(floyd::ApplyTask& apply);
    static bool RegisterReqidNotify(uint64_t reqid, const std::function<void (std::string& response)>& notify);
private:
    static Mcached mcached_;
    static moxie::Mutex notifyMutex_;
    static std::unordered_map<uint64_t, std::function<void (std::string& response)>> reqNotify_;
};

}

#endif 