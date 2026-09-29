// Unit tests of core::Stopwatch: the fake clock is injected through the Clock concept seam.
// The test moves time by hand, so no test sleeps and every expected duration is exact.
#include <gtest/gtest.h>

#include "core/clock.hpp"
#include "support/fake_clock.hpp"

#include <chrono>

namespace myproj::core {

using namespace std::chrono_literals;
using test_support::FakeClock;

TEST(Stopwatch, MeasuresTimeTheClockAdvanced) {
    FakeClock clock;
    const Stopwatch stopwatch(clock);

    clock.advance(250ns);

    EXPECT_EQ(stopwatch.elapsed(), 250ns);
}

TEST(Stopwatch, RestartMeasuresFromTheRestart) {
    FakeClock clock;
    Stopwatch stopwatch(clock);
    clock.advance(1s);

    stopwatch.restart();

    clock.advance(5ms);
    EXPECT_EQ(stopwatch.elapsed(), 5ms);
}

} // namespace myproj::core
