#include <gtest/gtest.h>

#include "Storage.hpp"

TEST(Storage, OpensInMemoryDatabase) {
    Storage s;
    EXPECT_TRUE(s.open(":memory:"));
}

TEST(Storage, InsertAndReadBackReadings) {
    Storage s;
    ASSERT_TRUE(s.open(":memory:"));
    EXPECT_TRUE(s.insertReading(1000, 500.0, 0.1));
    EXPECT_TRUE(s.insertReading(2000, 600.0, 0.2));

    auto rows = s.latestReadings(10);
    ASSERT_EQ(rows.size(), 2u);
    EXPECT_EQ(rows[0].timestamp_ns, 1000u);          // oldest first
    EXPECT_DOUBLE_EQ(rows[1].watts, 600.0);
}

TEST(Storage, LimitReturnsNewestRows) {
    Storage s;
    ASSERT_TRUE(s.open(":memory:"));
    for (int i = 1; i <= 5; ++i) s.insertReading(i, i * 100.0, i * 0.1);

    auto rows = s.latestReadings(2);
    ASSERT_EQ(rows.size(), 2u);
    EXPECT_EQ(rows[0].timestamp_ns, 4u);
    EXPECT_EQ(rows[1].timestamp_ns, 5u);
}

TEST(Storage, InsertAndReadBackAlerts) {
    Storage s;
    ASSERT_TRUE(s.open(":memory:"));
    EXPECT_TRUE(s.insertAlert(42, "High load: 3500 W"));

    auto alerts = s.latestAlerts(10);
    ASSERT_EQ(alerts.size(), 1u);
    EXPECT_EQ(alerts[0].message, "High load: 3500 W");
}

TEST(Storage, UnopenedStorageFailsSafely) {
    Storage s;                                       // never opened
    EXPECT_FALSE(s.insertReading(1, 1.0, 1.0));
    EXPECT_TRUE(s.latestReadings(5).empty());
}
