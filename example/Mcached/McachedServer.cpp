#include <McachedServer.h>

moxie::Mcached moxie::McachedServer::mcached_;
moxie::Mutex moxie::McachedServer::notifyMutex_;
std::unordered_map<uint64_t, std::function<void (std::string& response)>> moxie::McachedServer::reqNotify_;

void moxie::McachedServer::AfterAcceptSuccess(const std::shared_ptr<PollerEvent>& client, EventLoop *loop, const std::shared_ptr<moxie::NetAddress>& cad) {
    if (!loop->Register(client, std::make_shared<McachedClientHandler>(this, client, cad))) {
        ::close(client->GetFd());
        return;
    }
}

int moxie::McachedServer::ApplyMcached(std::vector<std::string>& args, std::string& response) {
    return mcached_.ExecuteCommand(args, response);
}

bool moxie::McachedServer::ApplyRaftTask(floyd::ApplyTask& apply) {
    std::string response;

    McachedServer::ApplyMcached(apply.argv, response);
    
    MutexLocker lock(McachedServer::notifyMutex_);
    uint64_t reqid = apply.reqid;
    if (McachedServer::reqNotify_.count(reqid) == 0) {
        return true;
    }

    auto notify = McachedServer::reqNotify_[reqid];
    if (notify) {
        notify(response);
    }
    return true;
}

bool moxie::McachedServer::RegisterReqidNotify(uint64_t reqid, const std::function<void (std::string& response)>& notify) {
    MutexLocker lock(McachedServer::notifyMutex_);
    if (McachedServer::reqNotify_.count(reqid) > 0) {
        return false;
    }

    McachedServer::reqNotify_[reqid] = notify;
    return true;
}

