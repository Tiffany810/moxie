#include <iostream>

#include <mcached/SlotObject.h>
#include <mcached/MutexLocker.h>

int moxie::SlotObject::ApplySet(const std::string& key, std::string& value, std::string& res) {
    db_[key] = std::move(value);
    res = "+OK\r\n";
    return moxie::kExecOk;
}

int moxie::SlotObject::ApplyGet(const std::string& key, std::string& res) {
    auto iter = db_.find(key);
    if (iter == db_.end()) {
        return moxie::kNotFoundKey;
    }

    res = iter->second;
    return moxie::kExecOk;
}
