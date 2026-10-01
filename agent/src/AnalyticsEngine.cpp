#include "AnalyticsEngine.hpp"

#include "EnergyCalculator.hpp"

AnalyticsEngine::AnalyticsEngine(int pulsesPerKwh, double windowSeconds, double alertWatts)
    : pulsesPerKwh_(pulsesPerKwh),
      windowSeconds_(windowSeconds),
      alertWatts_(alertWatts) {}

Reading AnalyticsEngine::update(const PulseEvent& event) {
    if (!hasBaseline_) {
        baselineCount_ = event.count;   // ignore pulses from before we started
        hasBaseline_ = true;
    }

    window_.push_back(event);

    // Drop old events that fall outside the time window.
    const double windowNs = windowSeconds_ * 1e9;
    while (window_.size() > 1 &&
           static_cast<double>(event.timestamp_ns - window_.front().timestamp_ns) > windowNs) {
        window_.pop_front();
    }

    // Power = pulses between oldest and newest event in the window.
    double watts = 0.0;
    if (window_.size() > 1) {
        const PulseEvent& first = window_.front();
        double pulses = static_cast<double>(event.count - first.count);
        double seconds = static_cast<double>(event.timestamp_ns - first.timestamp_ns) / 1e9;
        watts = EnergyCalculator::averageWatts(pulses, seconds, pulsesPerKwh_);
    }

    if (watts > peakWatts_) peakWatts_ = watts;

    Reading r;
    r.timestamp_ns = event.timestamp_ns;
    r.watts = watts;
    r.kwhTotal = EnergyCalculator::pulsesToKwh(
        static_cast<double>(event.count - baselineCount_), pulsesPerKwh_);
    r.alert = watts > alertWatts_;
    return r;
}
