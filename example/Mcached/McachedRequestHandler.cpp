#include <McachedRequestHandler.h>
#include <McachedServer.h>

#include <mraft/floyd/src/floyd_impl.h>

extern std::shared_ptr<floyd::FloydImpl> floyd_raft;

extern moxie::IdGenerator *igen;

moxie::McachedClientHandler::McachedClientHandler(McachedServer *server, const std::shared_ptr<PollerEvent>& client,  const std::shared_ptr<moxie::NetAddress>& cad) :
    server_(server),
    event_(client),
    peer_(cad),
    argc_(0),
    argv_() {
}

void moxie::McachedClientHandler::AfetrRead(const std::shared_ptr<PollerEvent>& event, EventLoop *loop) {
    if (ParseRedisRequest()) {
        this->DoMcachedCammand(loop);
    } 
    if (writeBuf_.readableBytes() > 0) {
        event_->EnableWrite();
        loop->Modity(event);
    }
}

void moxie::McachedClientHandler::AfetrWrite(const std::shared_ptr<PollerEvent>& event, EventLoop *loop) {
    if (writeBuf_.readableBytes() > 0) {
        return;
    }
    event->DisableWrite();
    loop->Modity(event);
}

void moxie::McachedClientHandler::MraftCallBack(const std::string& response, std::shared_ptr<McachedClientHandler> client) {
    client->et_->PushTask([response, client](EventLoop *loop){
        ResetArgcArgv reset(client->argc_, client->argv_);
        if (client->argv_[0] == "SET" || client->argv_[0] == "set") {
            client->ReplyString(response);
        } else if (client->argv_[0] == "get" || client->argv_[0] == "GET") {
            client->ReplyBulkString(response);
        }
        client->event_->EnableWrite();
        loop->Modity(client->event_);
    });
}

bool moxie::McachedClientHandler::DoFinallyMcached(EventLoop *loop, const std::string& response) {
    ResetArgcArgv reset(this->argc_, this->argv_);
    if (this->argv_[0] == "SET" || this->argv_[0] == "set") {
        this->ReplyString(response);
    } else if (this->argv_[0] == "get" || this->argv_[0] == "GET") {
        this->ReplyBulkString(response);
    }
    this->event_->EnableWrite();
    loop->Modity(this->event_);
    return true;
}

bool moxie::McachedClientHandler::DoMcachedCammand(EventLoop *loop) {
    assert(argc_ == argv_.size());
    assert(argc_ > 0);
    assert(floyd_raft);

    if (this->argv_[0] == "get" || this->argv_[0] == "GET") {
        std::string res;
        server_->ApplyMcached(this->argv_, res);
        this->DoFinallyMcached(loop, res);
        return true;
    }
    uint64_t reqid = igen->Next();
    auto succ = McachedServer::RegisterReqidNotify(reqid, std::bind(McachedClientHandler::MraftCallBack, std::placeholders::_1, shared_from_this()));
    if (!reqid) {
        // todo
        std::cout << "Bugs" << std::endl;
        return false;
    }
    floyd::RaftTask task;
    task.argv = this->argv_;
    task.reqid = reqid;
    floyd_raft->PushTask(task);
    return true;
}

bool moxie::McachedClientHandler::ParseRedisRequest() {
    if (readBuf_.readableBytes() <= 0) {
        return false;
    }

    if (argc_ <= 0) { // parse buffer to get argc
        argv_.clear();
        curArgvlen_ = -1;

        const char* crlf = readBuf_.findChars(Buffer::kCRLF, 2);
        if (crlf == nullptr) {
            return false;
        }

        std::string argc_str = readBuf_.retrieveAsString(crlf - readBuf_.peek());
        if (argc_str[0] != '*') {
            ReplyString("-ERR Protocol error: invalid multibulk length\r\n");
            return false;
        }

        argc_ = std::atoi(argc_str.c_str() + sizeof('*'));
        if (argc_ == 0) {
            ReplyString("-ERR Protocol error: invalid multibulk length\r\n");
            return false;
        }
        // \r\n
        readBuf_.retrieve(2);
    }
    
    while (argv_.size() < argc_) {
        if (curArgvlen_ < 0) {
            const char* crlf = readBuf_.findChars(Buffer::kCRLF, 2);
            if (crlf == nullptr) {
                return false;
            }
            std::string argv_len = readBuf_.retrieveAsString(crlf - readBuf_.peek());
            if (argv_len[0] != '$') {
                curArgvlen_ = -1;
                argc_ = 0;
                ReplyString("-ERR Protocol error: invalid bulk length (argv_len)\r\n");
                return false;
            }
            // remove $
            curArgvlen_ = std::atoi(argv_len.c_str() + sizeof('$'));
            readBuf_.retrieve(2);
        } else {
            if (readBuf_.readableBytes() < size_t(curArgvlen_ + 2)) {
                return false;
            }
            std::string argv_str = readBuf_.retrieveAsString(curArgvlen_);
            if (static_cast<ssize_t>(argv_str.size()) != curArgvlen_) {
                curArgvlen_ = -1;
                argc_ = 0;
                ReplyString("-ERR Protocol error: invalid bulk length (argv_str)\r\n");
                return false;
            }
            argv_.emplace_back(std::move(argv_str));
            // \r\n
            readBuf_.retrieve(2);
            curArgvlen_ = -1; // must do, to read next argv item
        }
    }
    // request can be solved
    if ((argc_ > 0) && (argv_.size() == argc_)) {
        return true;
    }

    return false;
}

void moxie::McachedClientHandler::DebugArgcArgv() const {
    std::cout << peer_->getIp() << ":" << peer_->getPort() << "->";
    for (size_t index = 0; index < argc_; index++) {
        std::cout << argv_[index];
        if (index != argc_ - 1) {
            std::cout << " ";
        }
    }
    std::cout << std::endl;
}

void moxie::McachedClientHandler::ReplyString(const std::string& error) {
    writeBuf_.append(error.c_str(), error.size());
}

void moxie::McachedClientHandler::ReplyBulkString(const std::string& item) {
    writeBuf_.append("$", 1);
    std::string len = std::to_string(item.size());
    writeBuf_.append(len.c_str(), len.size());
    writeBuf_.append(Buffer::kCRLF, 2);

    writeBuf_.append(item.c_str(), item.size());
    writeBuf_.append(Buffer::kCRLF, 2);
}
