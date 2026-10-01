#pragma once
#include <cstdint>
#include <mutex>

// The newest reading, shared between the analytics thread and web requests.
struct LiveState {
    bool hasData = false;
    std::uint64_t timestamp_ns = 0;
    double watts = 0.0;
    double kwhTotal = 0.0;
    double peakWatts = 0.0;
    bool alert = false;
};

class SharedState {
public:
    void set(const LiveState& s) {
        std::lock_guard<std::mutex> lock(mutex_);
        state_ = s;
    }
    LiveState get() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return state_;
    }

private:
    mutable std::mutex mutex_;
    LiveState state_;
};
