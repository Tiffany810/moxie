#ifndef MOXIE_CONF_H
#define MOXIE_CONF_H
#include <string>
#include <vector>
#include <iostream>

namespace moxie {

struct RaftConf {
    std::string cluster;
    std::string ip;
    int port;
    std::string data;
    bool vaild;

    void Show() {
        std::cout << "------------------------RaftConf------------------------" << std::endl;
        std::cout << "cluster:" << cluster << std::endl;
        std::cout << "ip:" << ip << std::endl;
        std::cout << "port:" << port << std::endl;
        std::cout << "data:" << data << std::endl;
        std::cout << "------------------------------------------------" << std::endl;
    }
};

struct McachedConf {
    std::string ip;
    short port;
    int threads;
    uint16_t id;
    std::string serverName;
    bool vaild;

    void Show() {
        std::cout << "------------------------McachedConf------------------------" << std::endl;
        std::cout << "id:" << id << std::endl;
        std::cout << "threads:" << threads << std::endl;
        std::cout << "ip:" << ip << std::endl;
        std::cout << "port:" << port << std::endl;
        std::cout << "serverName:" << serverName << std::endl;
        std::cout << "------------------------------------------------" << std::endl;
    }
};

struct ServiceConf {
    std::string ip;
    short port;
    std::string work_path;
    bool vaild;
    void Show() {
        std::cout << "------------------------McachedConf------------------------" << std::endl;
        std::cout << "ip:" << ip << std::endl;
        std::cout << "port:" << port << std::endl;
        std::cout << "work_path:" << work_path << std::endl;
        std::cout << "------------------------------------------------" << std::endl;
    }
};

struct ManagerServiceConf {
    std::vector<std::string> urls;
    std::string groupleeper;
    std::string slotkeeper;
    std::string keepalive;
    bool vaild;

    void Show() {
        std::cout << "------------------------McachedConf------------------------" << std::endl;
        //std::cout << "urls:" << urls << std::endl;
        std::cout << "groupleeper:" << groupleeper << std::endl;
        std::cout << "slotkeeper:" << slotkeeper << std::endl;
        std::cout << "keepalive:" << keepalive << std::endl;
        std::cout << "------------------------------------------------" << std::endl;
    }
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