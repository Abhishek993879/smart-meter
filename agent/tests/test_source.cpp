#include <gtest/gtest.h>

#include <atomic>

#include "SimulatedPulseSource.hpp"

TEST(SimulatedPulseSource, ProducesIncreasingCounts) {
    std::atomic<bool> running{true};
    // 360000 W at 1000 pulses/kWh = one pulse every 10 ms
    SimulatedPulseSource source(360000.0, 1000, running);

    PulseEvent a{}, b{};
    ASSERT_TRUE(source.next(a));
    ASSERT_TRUE(source.next(b));
    EXPECT_EQ(a.count, 1u);
    EXPECT_EQ(b.count, 2u);
    EXPECT_GT(b.timestamp_ns, a.timestamp_ns);
}

TEST(SimulatedPulseSource, StopsWhenNotRunning) {
    std::atomic<bool> running{false};
    SimulatedPulseSource source(1000.0, 1000, running);

    PulseEvent e{};
    EXPECT_FALSE(source.next(e));
}

TEST(SimulatedPulseSource, LoadChangeTakesEffectQuickly) {
    std::atomic<bool> running{true};
    SimulatedPulseSource source(1.0, 1000, running);   // very slow: one pulse per hour
    source.setWatts(360000.0);                         // now one pulse per 10 ms

    PulseEvent e{};
    ASSERT_TRUE(source.next(e));
    EXPECT_EQ(e.count, 1u);
}
