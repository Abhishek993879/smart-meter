#include <gtest/gtest.h>

#include "AnalyticsEngine.hpp"
#include "EnergyCalculator.hpp"

constexpr std::uint64_t SEC = 1000000000ULL;   // one second in nanoseconds

TEST(EnergyCalculator, PulsesToKwh) {
    EXPECT_DOUBLE_EQ(EnergyCalculator::pulsesToKwh(500, 1000), 0.5);
    EXPECT_DOUBLE_EQ(EnergyCalculator::pulsesToKwh(100, 0), 0.0);   // bad config
}

TEST(EnergyCalculator, AverageWatts) {
    // 10 pulses in 36 s at 1000 pulses/kWh = 1000 W
    EXPECT_NEAR(EnergyCalculator::averageWatts(10, 36, 1000), 1000.0, 1e-9);
    EXPECT_DOUBLE_EQ(EnergyCalculator::averageWatts(10, 0, 1000), 0.0);  // no divide by zero
}

TEST(EnergyCalculator, Cost) {
    EXPECT_DOUBLE_EQ(EnergyCalculator::cost(2.0, 8.0), 16.0);
}

TEST(AnalyticsEngine, FirstEventHasZeroPower) {
    AnalyticsEngine engine(1000, 60, 3000);
    Reading r = engine.update({0, 0});
    EXPECT_DOUBLE_EQ(r.watts, 0.0);
    EXPECT_FALSE(r.alert);
}

TEST(AnalyticsEngine, ComputesPowerFromTwoEvents) {
    AnalyticsEngine engine(1000, 60, 3000);
    engine.update({0, 0});
    Reading r = engine.update({36 * SEC, 10});
    EXPECT_NEAR(r.watts, 1000.0, 1e-6);
    EXPECT_NEAR(r.kwhTotal, 0.01, 1e-9);
}

TEST(AnalyticsEngine, RaisesAlertAboveThreshold) {
    AnalyticsEngine engine(1000, 60, 500);   // alert above 500 W
    engine.update({0, 0});
    Reading r = engine.update({36 * SEC, 10});   // 1000 W
    EXPECT_TRUE(r.alert);
}

TEST(AnalyticsEngine, NoAlertBelowThreshold) {
    AnalyticsEngine engine(1000, 60, 2000);
    engine.update({0, 0});
    Reading r = engine.update({36 * SEC, 10});   // 1000 W
    EXPECT_FALSE(r.alert);
}

TEST(AnalyticsEngine, OldEventsLeaveTheWindow) {
    AnalyticsEngine engine(1000, 60, 3000);
    engine.update({0, 0});
    engine.update({36 * SEC, 10});
    Reading r = engine.update({72 * SEC, 20});   // event at t=0 is now outside 60 s
    EXPECT_NEAR(r.watts, 1000.0, 1e-6);
    EXPECT_NEAR(r.kwhTotal, 0.02, 1e-9);
}

TEST(AnalyticsEngine, TracksPeakPower) {
    AnalyticsEngine engine(1000, 60, 9999);
    engine.update({0, 0});
    engine.update({36 * SEC, 10});               // 1000 W
    engine.update({72 * SEC, 12});               // slower: 2 pulses in 36 s
    EXPECT_NEAR(engine.peakWatts(), 1000.0, 1e-6);
}
