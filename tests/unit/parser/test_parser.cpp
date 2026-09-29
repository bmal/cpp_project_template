// Unit tests of parser::parse_line: output-based checks of fields and errors.
// One parameterized test covers every malformed shape; errors arrive as values, never as crashes.
#include <gtest/gtest.h>

#include "parser/parser.hpp"
#include "support/line_builder.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace myproj::parser {

TEST(ParseLine, SplitsBlankSeparatedFields) {
    const std::string line = test_support::LineBuilder{}.field("a", "1").field("b", "x=y").build();

    const auto fields = parse_line(line);

    ASSERT_TRUE(fields.has_value());
    EXPECT_EQ(*fields,
              (std::vector<Field>{{.key = "a", .value = "1"}, {.key = "b", .value = "x=y"}}));
}

TEST(ParseLine, BlankLineHasNoFields) {
    const auto fields = parse_line(" \t ");

    ASSERT_TRUE(fields.has_value());
    EXPECT_TRUE(fields->empty());
}

TEST(ParseLine, ErrorIsReturnedAsADescribableValue) {
    const auto fields = parse_line("a=1 b=");

    ASSERT_FALSE(fields.has_value());
    EXPECT_EQ(describe(fields.error().kind), "empty value after '='");
}

namespace {

struct MalformedLine {
    std::string_view name;
    std::string_view line;
    ParseError error;
};

class ParseLineRejects : public testing::TestWithParam<MalformedLine> {};

TEST_P(ParseLineRejects, TheFirstBadTokenWithItsColumn) {
    const auto fields = parse_line(GetParam().line);

    ASSERT_FALSE(fields.has_value());
    EXPECT_EQ(fields.error(), GetParam().error);
}

INSTANTIATE_TEST_SUITE_P(
    MalformedLines, ParseLineRejects,
    testing::Values(
        MalformedLine{"NoSeparator", "abc", {.kind = ErrorKind::MissingSeparator, .column = 0}},
        MalformedLine{"LoneSeparator", "=", {.kind = ErrorKind::EmptyKey, .column = 0}},
        MalformedLine{"EmptyKeyAfterAField", "a=1 =2", {.kind = ErrorKind::EmptyKey, .column = 4}},
        MalformedLine{"EmptyValue", "a= b=2", {.kind = ErrorKind::EmptyValue, .column = 0}},
        MalformedLine{
            "ColumnCountsBlanks", "\t\tx", {.kind = ErrorKind::MissingSeparator, .column = 2}},
        MalformedLine{"FirstErrorWins", "=1 b", {.kind = ErrorKind::EmptyKey, .column = 0}}),
    [](const testing::TestParamInfo<MalformedLine>& param) {
        return std::string(param.param.name);
    });

} // namespace

} // namespace myproj::parser
