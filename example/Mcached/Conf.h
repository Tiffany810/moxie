#ifndef MOXIE_CONF_H
#define MOXIE_CONF_H
#include <string>
#include <vector>

namespace moxie {

struct RaftConf {
    std::string cluster;
    std::string ip;
    int port;
    std::string data;
    bool vaild;
};

struct McachedConf {
    std::string ip;
    short port;
    int threads;
    uint16_t id;
    std::string serverName;
    bool vaild;
};

struct ServiceConf {
    std::string ip;
    short port;
    std::string work_path;
    bool vaild;
};

struct ManagerServiceConf {
    std::vector<std::string> urls;
    std::string groupleeper;
    std::string slotkeeper;
    std::string keepalive;
    bool vaild;
};

class Conf {
public:
    bool ParseConf(const std::string& conf);
    McachedConf GetMcachedConf() const {
        return mcachedConf_;
    }
    ServiceConf GetServiceConf() const {
        return serviceConf_;
    }
    RaftConf GetRaftConf() const {
        return raftConf_;
    }
    ManagerServiceConf GetManagerServiceConf() const {
        return managerServiceConf_;
    }
private:
    McachedConf mcachedConf_;
    ServiceConf serviceConf_;
    ManagerServiceConf managerServiceConf_;
    RaftConf raftConf_;
};

}

#endif