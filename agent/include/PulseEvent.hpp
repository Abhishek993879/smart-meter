#pragma once
#include <cstdint>

// One reading from the pulse counter.
struct PulseEvent {
    std::uint64_t timestamp_ns;  // time of the pulse, nanoseconds
    std::uint64_t count;         // total pulses since the counter started
};
