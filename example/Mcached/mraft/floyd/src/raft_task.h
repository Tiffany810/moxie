#ifndef FLOYD_MRAFTTASK_H
#define FLOYD_MRAFTTASK_H
#include <vector>
#include <string>
#include <functional>

namespace floyd {

struct RaftTask {
    std::vector<std::string> argv;
    uint64_t reqid;
    
};

struct ApplyTask {
    uint64_t reqid;
    bool succeed;
    std::vector<std::string> argv;
};

}

#endif