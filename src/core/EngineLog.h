#pragma once
#include <string>
#include <vector>

// Global log buffer. Call EngineLog::Log() from anywhere to append a message.
// Messages are printed to stdout and stored for the in-game LogConsole overlay.
class EngineLog {
public:
    static void Log(const std::string& message);
    static const std::vector<std::string>& GetMessages();
    static void Clear();

private:
    static constexpr size_t MAX_MESSAGES = 500;
    static std::vector<std::string> messages_;
};
