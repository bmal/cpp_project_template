// Functional tests of myproj_cli as a black box: stdin in, exit code and output out.
// They know the app only by its path and its documented behavior, never by its code.
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "cli_process.hpp"

namespace myproj::functional {

using testing::HasSubstr;
using testing::StartsWith;

TEST(Cli, PrintsTheBannerThenEachFieldAndExitsZero) {
    const auto run = run_cli("a=1 b=2\n");

    ASSERT_TRUE(run.has_value()) << run.error();
    EXPECT_EQ(run->exit_code, 0);
    EXPECT_THAT(run->out, StartsWith("myproj "));
    EXPECT_THAT(run->out, HasSubstr("\na: 1\nb: 2\n"));
}

TEST(Cli, ReportsTheLineAndFieldCountsOnStandardError) {
    const auto run = run_cli("a=1\nb=2 c=3\n");

    ASSERT_TRUE(run.has_value()) << run.error();
    EXPECT_EQ(run->exit_code, 0);
    EXPECT_THAT(run->err, StartsWith("2 lines, 3 fields in "));
}

TEST(Cli, ExitsOneAndNamesTheLineAndColumnOfAMalformedLine) {
    const auto run = run_cli("a=1\nb=2 c\n");

    ASSERT_TRUE(run.has_value()) << run.error();
    EXPECT_EQ(run->exit_code, 1);
    EXPECT_EQ(run->err, "line 2, column 5: missing '=' between key and value\n");
}

} // namespace myproj::functional
