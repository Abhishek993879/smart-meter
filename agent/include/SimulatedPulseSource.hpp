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

private:
    const std::atomic<bool>& running_;
    std::chrono::nanoseconds interval_;
    std::chrono::steady_clock::time_point nextPulse_;
    std::uint64_t count_ = 0;
};
