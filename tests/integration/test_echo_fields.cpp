// Integration tests of core::echo_fields over the real parser, with a GoogleMock at the sink.
// The sink is the one unmanaged edge, what the user sees; everything behind it runs for real.
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "core/echo_fields.hpp"
#include "support/line_builder.hpp"

#include <cstddef>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

namespace myproj::core {

namespace {

using test_support::LineBuilder;
using testing::StrictMock;

class MockOutputSink : public OutputSink {
public:
    MOCK_METHOD(void, field, (std::string_view key, std::string_view value), (override));
};

} // namespace

TEST(EchoFields, SendsEveryFieldToTheSinkInInputOrder) {
    StrictMock<MockOutputSink> sink;
    std::istringstream input(LineBuilder{}.field("a", "1").field("b", "2").build() + "\n" +
                             LineBuilder{}.field("c", "3").build() + "\n");
    const testing::InSequence in_order;
    EXPECT_CALL(sink, field("a", "1"));
    EXPECT_CALL(sink, field("b", "2"));
    EXPECT_CALL(sink, field("c", "3"));

    const auto totals = echo_fields(input, sink);

    ASSERT_TRUE(totals.has_value());
    EXPECT_EQ(totals->lines, 2);
    EXPECT_EQ(totals->fields, 3);
}

TEST(EchoFields, StopsAtTheFirstMalformedLineAndReportsWhereItIs) {
    StrictMock<MockOutputSink> sink;
    std::istringstream input("a=1\nb=2 c\nd=4\n");
    EXPECT_CALL(sink, field("a", "1"));

    const auto totals = echo_fields(input, sink);

    ASSERT_FALSE(totals.has_value());
    EXPECT_EQ(totals.error().line, 2);
    EXPECT_EQ(totals.error().column, 5);
    EXPECT_EQ(totals.error().message, "missing '=' between key and value");
}

TEST(EchoFieldsStress, EchoesAMillionFields) {
    constexpr std::size_t line_count = 250'000;
    const std::string line =
        LineBuilder{}.field("a", "1").field("b", "2").field("c", "3").field("d", "4").build() +
        "\n";
    std::string text;
    for (std::size_t i = 0; i < line_count; ++i) {
        text += line;
    }
    std::istringstream input(std::move(text));
    StrictMock<MockOutputSink> sink;
    EXPECT_CALL(sink, field).Times(4 * line_count);

    const auto totals = echo_fields(input, sink);

    ASSERT_TRUE(totals.has_value());
    EXPECT_EQ(totals->fields, 4 * line_count);
}

} // namespace myproj::core
