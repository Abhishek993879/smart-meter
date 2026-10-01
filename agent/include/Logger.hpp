#pragma once
#include <fstream>
#include <mutex>
#include <string>

enum class LogLevel { Debug, Info, Warn, Error };

class Logger {
public:
    static Logger& instance();           // one shared logger for the whole program

    void setLevel(LogLevel level);
    bool openFile(const std::string& path);
    void log(LogLevel level, const std::string& message);

private:
    Logger() = default;
    std::mutex mutex_;                   // so threads don't mix their lines
    std::ofstream file_;
    LogLevel level_ = LogLevel::Info;
};

void logDebug(const std::string& msg);
void logInfo(const std::string& msg);
void logWarn(const std::string& msg);
void logError(const std::string& msg);
