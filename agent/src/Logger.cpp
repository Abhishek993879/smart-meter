#include "Logger.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>

Logger& Logger::instance() {
    static Logger logger;                // created once, thread-safe in C++11+
    return logger;
}

void Logger::setLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    level_ = level;
}

bool Logger::openFile(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    file_.open(path, std::ios::app);
    return file_.is_open();
}

static const char* levelName(LogLevel level) {
    switch (level) {
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info:  return "INFO ";
        case LogLevel::Warn:  return "WARN ";
        case LogLevel::Error: return "ERROR";
    }
    return "?????";
}

void Logger::log(LogLevel level, const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (level < level_) return;

    std::time_t now = std::chrono::system_clock::to_time_t(
        std::chrono::system_clock::now());
    std::tm tm_buf{};
    localtime_r(&now, &tm_buf);

    std::ostringstream line;
    line << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S")
         << " [" << levelName(level) << "] " << message;

    std::cout << line.str() << std::endl;
    if (file_.is_open()) file_ << line.str() << std::endl;
}

void logDebug(const std::string& msg) { Logger::instance().log(LogLevel::Debug, msg); }
void logInfo(const std::string& msg)  { Logger::instance().log(LogLevel::Info, msg); }
void logWarn(const std::string& msg)  { Logger::instance().log(LogLevel::Warn, msg); }
void logError(const std::string& msg) { Logger::instance().log(LogLevel::Error, msg); }
