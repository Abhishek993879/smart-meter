#pragma once
#include <cstdint>
#include <deque>

#include "PulseEvent.hpp"

// Result of processing one event.
struct Reading {
    std::uint64_t timestamp_ns;
    double watts;       // average power over the sliding window
    double kwhTotal;    // energy since the engine saw its first event
    bool alert;         // true if watts is above the alert threshold
};

class AnalyticsEngine {
public:
    AnalyticsEngine(int pulsesPerKwh, double windowSeconds, double alertWatts);

    Reading update(const PulseEvent& event);
    double peakWatts() const { return peakWatts_; }

private:
    int pulsesPerKwh_;
    double windowSeconds_;
    double alertWatts_;

    std::deque<PulseEvent> window_;
    bool hasBaseline_ = false;
    std::uint64_t baselineCount_ = 0;
    double peakWatts_ = 0.0;
};
