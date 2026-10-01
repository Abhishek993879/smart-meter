#pragma once
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

struct sqlite3;   // forward declaration, so users of Storage don't need sqlite3.h

struct StoredReading {
    std::uint64_t timestamp_ns;
    double watts;
    double kwhTotal;
};

struct StoredAlert {
    std::uint64_t timestamp_ns;
    std::string message;
};

// Wraps one SQLite database. Safe to use from several threads.
class Storage {
public:
    Storage() = default;
    ~Storage();
    Storage(const Storage&) = delete;              // owns a DB handle: no copying
    Storage& operator=(const Storage&) = delete;

    bool open(const std::string& path);            // use ":memory:" for tests
    bool insertReading(std::uint64_t ts, double watts, double kwhTotal);
    bool insertAlert(std::uint64_t ts, const std::string& message);

    // Oldest-first, so they are ready to draw on a chart.
    std::vector<StoredReading> latestReadings(int limit);
    std::vector<StoredAlert> latestAlerts(int limit);

private:
    bool exec(const char* sql);                    // caller must hold the mutex

    sqlite3* db_ = nullptr;
    std::mutex mutex_;
};
