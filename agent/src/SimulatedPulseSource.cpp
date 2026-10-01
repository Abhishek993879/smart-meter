#include "SimulatedPulseSource.hpp"

#include <algorithm>
#include <thread>

#include "Clock.hpp"

SimulatedPulseSource::SimulatedPulseSource(double watts, int pulsesPerKwh,
                                           const std::atomic<bool>& running)
    : running_(running) {
    double w = std::max(watts, 1.0);                 // avoid divide by zero
    double ppk = std::max(pulsesPerKwh, 1);
    double seconds = 3600000.0 / (w * ppk);
    interval_ = std::chrono::nanoseconds(static_cast<long long>(seconds * 1e9));
    nextPulse_ = std::chrono::steady_clock::now() + interval_;
}

bool SimulatedPulseSource::next(PulseEvent& event) {
    using clock = std::chrono::steady_clock;
    const auto slice = std::chrono::milliseconds(50);   // wake often to notice shutdown

    while (running_) {
        auto now = clock::now();
        if (now >= nextPulse_) {
            nextPulse_ += interval_;
            ++count_;
            event = PulseEvent{nowNs(), count_};
            return true;
        }
        std::this_thread::sleep_for(std::min<clock::duration>(slice, nextPulse_ - now));
    }
    return false;
}
