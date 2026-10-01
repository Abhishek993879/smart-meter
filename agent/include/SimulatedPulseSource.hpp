#pragma once
#include <atomic>
#include <chrono>
#include <cstdint>

#include "PulseSource.hpp"

// Generates pulses as a meter would for a constant load.
class SimulatedPulseSource : public PulseSource {
public:
    SimulatedPulseSource(double watts, int pulsesPerKwh, const std::atomic<bool>& running);
    bool next(PulseEvent& event) override;

    // Change the load while running. Safe to call from another thread.
    void setWatts(double watts);

private:
    std::chrono::nanoseconds currentInterval() const;

    const std::atomic<bool>& running_;
    std::atomic<double> watts_;
    std::atomic<bool> loadChanged_{false};
    int pulsesPerKwh_;
    std::chrono::steady_clock::time_point lastPulse_;
    std::uint64_t count_ = 0;
};
