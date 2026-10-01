#include "SimulatedPulseSource.hpp"

#include <algorithm>
#include <thread>

#include "Clock.hpp"

SimulatedPulseSource::SimulatedPulseSource(double watts, int pulsesPerKwh,
                                           const std::atomic<bool>& running)
    : running_(running),
      watts_(std::max(watts, 1.0)),
      pulsesPerKwh_(std::max(pulsesPerKwh, 1)),
      lastPulse_(std::chrono::steady_clock::now()) {}

void SimulatedPulseSource::setWatts(double watts) {
    watts_ = std::max(watts, 1.0);
    loadChanged_ = true;
}

std::chrono::nanoseconds SimulatedPulseSource::currentInterval() const {
    double seconds = 3600000.0 / (watts_.load() * pulsesPerKwh_);
    return std::chrono::nanoseconds(static_cast<long long>(seconds * 1e9));
}

bool SimulatedPulseSource::next(PulseEvent& event) {
    using clock = std::chrono::steady_clock;
    const auto slice = std::chrono::milliseconds(50);   // wake often: notice shutdown and load changes

    while (running_) {
        // After a load change, count the next interval from now. Without this,
        // a jump from a slow load to a fast one would release a burst of pulses.
        if (loadChanged_.exchange(false)) lastPulse_ = clock::now();

        auto due = lastPulse_ + currentInterval();
        auto now = clock::now();
        if (now >= due) {
            lastPulse_ = due;
            ++count_;
            event = PulseEvent{nowNs(), count_};
            return true;
        }
        std::this_thread::sleep_for(std::min<clock::duration>(slice, due - now));
    }
    return false;
}
