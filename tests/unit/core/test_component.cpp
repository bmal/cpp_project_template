#include <gtest/gtest.h>
#include "core/component.hpp"

namespace myproj {

TEST(Counter, IncrementAccumulates) {
    Counter c;
    c.increment().increment(3).increment();
    EXPECT_EQ(c.get(), 5);
}

TEST(Counter, DecrementReducesValue) {
    Counter c;
    c.increment(10).decrement(3);
    EXPECT_EQ(c.get(), 7);
}

TEST(Counter, ResetRestoresZero) {
    Counter c;
    c.increment(10);
    c.reset();
    EXPECT_EQ(c.get(), 0);
}

TEST(Counter, CheckedDecrementReturnsNewValueOnSuccess) {
    Counter c;
    c.increment(5);
    const auto result = c.checked_decrement(3);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 2);
}

TEST(Counter, CheckedDecrementReturnsErrorOnUnderflow) {
    Counter c;
    c.increment(2);
    const auto result = c.checked_decrement(5);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), "underflow: result would be negative");
}

}  // namespace myproj
