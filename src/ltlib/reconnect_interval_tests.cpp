#include <array>

#include <gtest/gtest.h>

#include <ltlib/reconnect_interval.h>

namespace {

TEST(ReconnectIntervalTest, FirstValueIsInitialBackoff) {
    ltlib::ReconnectInterval reconnect;
    EXPECT_EQ(reconnect.next(), 100);
}

TEST(ReconnectIntervalTest, SequenceSaturatesAtUpperBound) {
    ltlib::ReconnectInterval reconnect;
    constexpr std::array<int64_t, 8> kExpected{100, 500, 1000, 2000, 5000, 10000, 30000, 60000};

    for (size_t i = 0; i < kExpected.size(); ++i) {
        EXPECT_EQ(reconnect.next(), kExpected[i]);
    }

    for (int i = 0; i < 16; ++i) {
        EXPECT_EQ(reconnect.next(), 60000);
    }
}

TEST(ReconnectIntervalTest, ResetRestartsFromFirstValue) {
    ltlib::ReconnectInterval reconnect;
    (void)reconnect.next();
    (void)reconnect.next();
    (void)reconnect.next();

    reconnect.reset();

    EXPECT_EQ(reconnect.next(), 100);
    EXPECT_EQ(reconnect.next(), 500);
}

} // namespace