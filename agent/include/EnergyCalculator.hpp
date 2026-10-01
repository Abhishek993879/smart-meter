#pragma once

// Pure functions: no state, no I/O, easy to test.
namespace EnergyCalculator {

inline double pulsesToKwh(double pulses, int pulsesPerKwh) {
    if (pulsesPerKwh <= 0) return 0.0;
    return pulses / pulsesPerKwh;
}

// Average power in watts for 'pulses' counted over 'seconds'.
inline double averageWatts(double pulses, double seconds, int pulsesPerKwh) {
    if (seconds <= 0.0) return 0.0;
    double kwh = pulsesToKwh(pulses, pulsesPerKwh);
    return kwh * 1000.0 * 3600.0 / seconds;
}

inline double cost(double kwh, double tariffPerKwh) {
    return kwh * tariffPerKwh;
}

}  // namespace EnergyCalculator
