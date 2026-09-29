// Unit tests of core::Counter: state-based checks through the public interface only.
// The builder arranges a total, and the allocation guard proves the hot path never allocates.
#include <gtest/gtest.h>

#include "core/counter.hpp"
#include "support/allocation_guard.hpp"
#include "support/counter_builder.hpp"

#include <cstdint>
#include <expected>

namespace myproj::core {

using test_support::CounterBuilder;

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
    Counter counter = CounterBuilder{}.with_total(10).build();

    const auto result = counter.subtract(3);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 7);
}

TEST(Counter, SubtractBelowZeroFailsAndKeepsTheTotal) {
    Counter counter = CounterBuilder{}.with_total(2).build();

    const auto result = counter.subtract(5);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), CounterError::Underflow);
    EXPECT_EQ(counter.value(), 2);
}

TEST(Counter, ResetReturnsToZero) {
    Counter counter = CounterBuilder{}.with_total(10).build();

    counter.reset();

    EXPECT_EQ(counter.value(), 0);
}

TEST(Counter, SubtractDoesNotAllocate) {
    Counter counter = CounterBuilder{}.with_total(10).build();
    const test_support::AllocationGuard guard;

    const std::expected<std::uint64_t, CounterError> result = counter.subtract(3);

    EXPECT_EQ(result, 7);
}

} // namespace myproj::core
