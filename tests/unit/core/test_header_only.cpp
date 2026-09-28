#include <gtest/gtest.h>
#include "core/header_only.hpp"

namespace myproj {

TEST(SafeDivide, ReturnsCorrectValueOnSuccess) {
    const auto result = safe_divide(10, 2);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result.value(), 5);
}

TEST(SafeDivide, ReturnsExpectedErrorOnDivisionByZero) {
    const auto result = safe_divide(10, 0);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), "division by zero");
}

}  // namespace myproj
