#include <fstream>

#include <json/json.h>

#include <Conf.h>

bool moxie::Conf::ParseConf(const std::string& conf) {
    Json::Reader reader;  
    Json::Value root;
    std::ifstream is;
    is.open(conf, std::ios::binary);
    if (!reader.parse(is, root)) {  
        return false;
    }
    if (!root["service"].isNull() || root["service"].isObject()) {
        this->serviceConf_.vaild = true;
        auto service = root["service"];
        if (service.isMember("ip") && service["ip"].isString()) {
            this->serviceConf_.ip = service["ip"].asString();
        }
        if (service.isMember("port") && service["port"].isInt()) {
            this->serviceConf_.port = service["port"].asInt();
        }
        if (service.isMember("work_path") && service["work_path"].isString()) {
            this->serviceConf_.ip = service["work_path"].asString();
        }
    } else {
        this->serviceConf_.vaild = false;
    }

    if (!root["raft"].isNull() || root["raft"].isObject()) {
        this->raftConf_.vaild = true;
        auto raft = root["raft"];
        if (raft.isMember("cluster") && raft["cluster"].isString()) {
            this->raftConf_.cluster = raft["cluster"].asString();
        }
        if (raft.isMember("ip") && raft["ip"].isString()) {
            this->raftConf_.ip = raft["ip"].asString();
        }
        if (raft.isMember("port") && raft["port"].isInt()) {
            this->raftConf_.port = raft["port"].asInt();
        }
        if (raft.isMember("data") && raft["data"].isString()) {
            this->raftConf_.data = raft["data"].asString();
        }
    } else {
        this->raftConf_.vaild = false;
    }

    if (!root["mcached"].isNull() && root["mcached"].isObject()) {
        this->mcachedConf_.vaild = true;
        auto mcached = root["mcached"];
        if (mcached.isMember("ip") && mcached["ip"].isString()) {
            this->mcachedConf_.ip = mcached["ip"].asString();
        }
        if (mcached.isMember("server_name") && mcached["server_name"].isString()) {
            this->mcachedConf_.serverName = mcached["server_name"].asString();
        }
        if (mcached.isMember("port") && mcached["port"].isInt()) {
            this->mcachedConf_.port= mcached["port"].asInt();
        }
        if (mcached.isMember("threads") && mcached["threads"].isInt()) {
            this->mcachedConf_.threads = mcached["threads"].asInt();
        }
        if (mcached.isMember("id") && mcached["id"].isInt()) {
            this->mcachedConf_.id = mcached["id"].asInt();
        }          
    } else {
        this->mcachedConf_.vaild = false;
    }

    if (!root["manager_service"].isNull() && root["manager_service"].isObject()) {
        this->managerServiceConf_.vaild = true;
        auto service = root["manager_service"];
        if (service.isMember("urls") && service["urls"].isArray()) {
            auto urls = service["urls"];
            for (uint i = 0; i < urls.size(); i++) {
                this->managerServiceConf_.urls.push_back(urls[i].asString()); 
            } 
        }
        if (service.isMember("groupkeeper_path") && service["groupkeeper_path"].isString()) {
            this->managerServiceConf_.groupleeper = service["groupkeeper_path"].asString();
        }
        if (service.isMember("keepalive_path") && service["keepalive_path"].isString()) {
            this->managerServiceConf_.keepalive = service["keepalive_path"].asString();
        }
        if (service.isMember("slotkeeper_path") && service["slotkeeper_path"].isString()) {
            this->managerServiceConf_.slotkeeper = service["slotkeeper_path"].asString();
        }        
    } else {
        this->managerServiceConf_.vaild = false;
    }

    this->raftConf_.Show();
    this->managerServiceConf_.Show();
    this->mcachedConf_.Show();
    this->serviceConf_.Show();

    return true;
}