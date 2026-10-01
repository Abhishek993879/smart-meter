#pragma once
#include <chrono>
#include <cstdint>

// Wall-clock time in nanoseconds since 1970, so the dashboard can show real dates.
inline std::uint64_t nowNs() {
    using namespace std::chrono;
    return static_cast<std::uint64_t>(
        duration_cast<nanoseconds>(system_clock::now().time_since_epoch()).count());
}
