#include "core/EngineLog.h"
#include <cstdio>

std::vector<std::string> EngineLog::messages_;

void EngineLog::Log(const std::string& message) {
    std::printf("%s\n", message.c_str());
    messages_.push_back(message);
    if (messages_.size() > MAX_MESSAGES)
        messages_.erase(messages_.begin());
}

const std::vector<std::string>& EngineLog::GetMessages() {
    return messages_;
}

void EngineLog::Clear() {
    messages_.clear();
}
