// Unit tests of parser::parse_line: output-based checks of fields and errors.
// Parameterized cases for every malformed shape arrive with the test strategy examples.
#include <gtest/gtest.h>

#include "parser/parser.hpp"

#include <vector>

namespace myproj::parser {

TEST(ParseLine, SplitsBlankSeparatedFields) {
    const auto fields = parse_line("a=1  b=x=y\t");

    ASSERT_TRUE(fields.has_value());
    EXPECT_EQ(*fields, (std::vector<Field>{{.key = "a", .value = "1"}, {.key = "b", .value = "x=y"}}));
}

TEST(ParseLine, BlankLineHasNoFields) {
    const auto fields = parse_line("   ");

    ASSERT_TRUE(fields.has_value());
    EXPECT_TRUE(fields->empty());
}

TEST(ParseLine, LoneSeparatorIsAnEmptyKey) {
    const auto fields = parse_line("=");

    ASSERT_FALSE(fields.has_value());
    EXPECT_EQ(fields.error(), (ParseError{.kind = ErrorKind::EmptyKey, .column = 0}));
}

TEST(ParseLine, TokenWithoutSeparatorReportsItsColumn) {
    const auto fields = parse_line("a=1 oops");

    ASSERT_FALSE(fields.has_value());
    EXPECT_EQ(fields.error(), (ParseError{.kind = ErrorKind::MissingSeparator, .column = 4}));
}

} // namespace myproj::parser
