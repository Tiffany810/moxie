#ifndef MOXIE_MCACHED
#define MOXIE_MCACHED
#include <vector>
#include <string>
#include <memory>

#include <mcached/SlotObject.h>
#include <mcached/Mutex.h>
#include <mcached/MutexLocker.h>
#include <google/protobuf/repeated_field.h>

namespace moxie { 

class Mcached {
public:
    Mcached();
    ~Mcached();
    int ExecuteCommand(std::vector<std::string>& args, std::string& response);
    int ExecuteCommand(::google::protobuf::RepeatedPtrField<::std::string>& args, std::string& response);
    static bool IsReadOnly(const std::string& cmdname);
    static size_t BKDRHash(const char *str);
public:
    template <class T>
    static bool CheckSetArgs(const T& args) {
        if (args.size() < 3) {
            return false;
        }
        return true;
    }
    template <class T>
    static bool CheckGetArgs(const T& args) {
        if (args.size() < 2) {
            return false;
        }
        return true;
    }
private:
    template <class T>
    int InnerExecuteCommand(T& args, std::string& response) {
        if (args.size() == 0) {
            return moxie::kExecArgsError;
        }
        std::string cmd = args[0];
        if (cmd == "set" || cmd == "SET") {
            if (CheckSetArgs(args)) {
                SlotObject *slot = slots_[BKDRHash(args[1].c_str()) % kSlotNum];
                return slot->ApplySet(args[1], args[2], response);
            } else {
                return moxie::kExecArgsError;
            }
        } else if (cmd == "get" || cmd == "GET") {
            if (CheckGetArgs(args)) {
                SlotObject *slot = slots_[BKDRHash(args[1].c_str()) % kSlotNum];
                return slot->ApplyGet(args[1], response);
            } else {
                return moxie::kExecArgsError;
            }
        } 
        return moxie::kExecArgsError;
    } 
private:
    std::vector<SlotObject*> slots_;
    moxie::Mutex mutex_;
    static const size_t kSlotNum = 1024;
};

}

#endif
