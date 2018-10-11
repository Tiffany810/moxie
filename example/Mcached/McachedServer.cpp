#include <McachedServer.h>

void moxie::McachedServer::AfterAcceptSuccess(const std::shared_ptr<PollerEvent>& client, EventLoop *loop, const std::shared_ptr<moxie::NetAddress>& cad) {
    if (!loop->Register(client, std::make_shared<McachedClientHandler>(this, client, cad))) {
        ::close(client->GetFd());
        return;
    }
}

int moxie::McachedServer::ApplyMcached(std::vector<std::string>& args, std::string& response) {
    return mcached_.ExecuteCommand(args, response);
}
