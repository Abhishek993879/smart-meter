#include <gtest/gtest.h>

#include "SharedState.hpp"

TEST(SharedState, StartsEmpty) {
    SharedState s;
    EXPECT_FALSE(s.get().hasData);
}

TEST(SharedState, StoresLatestValue) {
    SharedState s;
    LiveState l;
    l.hasData = true;
    l.watts = 1234.5;
    s.set(l);

    LiveState g = s.get();
    EXPECT_TRUE(g.hasData);
    EXPECT_DOUBLE_EQ(g.watts, 1234.5);
}
