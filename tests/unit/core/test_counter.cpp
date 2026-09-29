// Unit tests of core::Counter: state-based checks through the public interface only.
// The strategy examples with the fake clock and the allocation guard arrive with the test support library.
#include <gtest/gtest.h>

#include "core/counter.hpp"

namespace myproj::core {

TEST(Counter, StartsAtZero) {
    const Counter counter;

    EXPECT_EQ(counter.value(), 0);
}

TEST(Counter, AddAccumulates) {
    Counter counter;

    counter.add();
    counter.add(3);

    EXPECT_EQ(counter.value(), 4);
}

TEST(Counter, SubtractReturnsTheNewTotal) {
    Counter counter;
    counter.add(10);

    const auto result = counter.subtract(3);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 7);
}

TEST(Counter, SubtractBelowZeroFailsAndKeepsTheTotal) {
    Counter counter;
    counter.add(2);

    const auto result = counter.subtract(5);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), CounterError::Underflow);
    EXPECT_EQ(counter.value(), 2);
}

TEST(Counter, ResetReturnsToZero) {
    Counter counter;
    counter.add(10);

    counter.reset();

    EXPECT_EQ(counter.value(), 0);
}

} // namespace myproj::core
