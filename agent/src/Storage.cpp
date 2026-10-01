#include "Storage.hpp"

#include <sqlite3.h>

#include <algorithm>

#include "Logger.hpp"

Storage::~Storage() {
    if (db_) sqlite3_close(db_);
}

bool Storage::exec(const char* sql) {
    char* err = nullptr;
    if (sqlite3_exec(db_, sql, nullptr, nullptr, &err) != SQLITE_OK) {
        logError(std::string("SQLite error: ") + (err ? err : "unknown"));
        sqlite3_free(err);
        return false;
    }
    return true;
}

bool Storage::open(const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (sqlite3_open(path.c_str(), &db_) != SQLITE_OK) {
        logError("Cannot open database: " + path);
        sqlite3_close(db_);
        db_ = nullptr;
        return false;
    }
    return exec("CREATE TABLE IF NOT EXISTS readings("
                "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                "ts_ns INTEGER NOT NULL, watts REAL NOT NULL, kwh_total REAL NOT NULL);") &&
           exec("CREATE TABLE IF NOT EXISTS alerts("
                "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                "ts_ns INTEGER NOT NULL, message TEXT NOT NULL);");
}

bool Storage::insertReading(std::uint64_t ts, double watts, double kwhTotal) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return false;

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_,
            "INSERT INTO readings(ts_ns, watts, kwh_total) VALUES(?,?,?);",
            -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_int64(stmt, 1, static_cast<sqlite3_int64>(ts));
    sqlite3_bind_double(stmt, 2, watts);
    sqlite3_bind_double(stmt, 3, kwhTotal);
    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

bool Storage::insertAlert(std::uint64_t ts, const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!db_) return false;

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_,
            "INSERT INTO alerts(ts_ns, message) VALUES(?,?);",
            -1, &stmt, nullptr) != SQLITE_OK) return false;

    sqlite3_bind_int64(stmt, 1, static_cast<sqlite3_int64>(ts));
    sqlite3_bind_text(stmt, 2, message.c_str(), -1, SQLITE_TRANSIENT);
    bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    sqlite3_finalize(stmt);
    return ok;
}

std::vector<StoredReading> Storage::latestReadings(int limit) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<StoredReading> out;
    if (!db_) return out;

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_,
            "SELECT ts_ns, watts, kwh_total FROM readings "
            "ORDER BY id DESC LIMIT ?;",
            -1, &stmt, nullptr) != SQLITE_OK) return out;

    sqlite3_bind_int(stmt, 1, limit);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        out.push_back({static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 0)),
                       sqlite3_column_double(stmt, 1),
                       sqlite3_column_double(stmt, 2)});
    }
    sqlite3_finalize(stmt);
    std::reverse(out.begin(), out.end());   // newest-first -> oldest-first
    return out;
}

std::vector<StoredAlert> Storage::latestAlerts(int limit) {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<StoredAlert> out;
    if (!db_) return out;

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_,
            "SELECT ts_ns, message FROM alerts ORDER BY id DESC LIMIT ?;",
            -1, &stmt, nullptr) != SQLITE_OK) return out;

    sqlite3_bind_int(stmt, 1, limit);
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        out.push_back({static_cast<std::uint64_t>(sqlite3_column_int64(stmt, 0)),
                       text ? text : ""});
    }
    sqlite3_finalize(stmt);
    std::reverse(out.begin(), out.end());
    return out;
}
