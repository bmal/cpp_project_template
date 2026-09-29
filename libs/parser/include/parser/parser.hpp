// Mechanism: a header-only, exception-free module; every error travels in std::expected.
// A key=value line tokenizer, small and pure enough to fuzz and benchmark in isolation.
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <string_view>
#include <vector>

namespace myproj::parser {

// One key=value token. Both views point into the parsed line.
struct Field {
    std::string_view key;
    std::string_view value;

    friend bool operator==(const Field&, const Field&) = default;
};

enum class ErrorKind : std::uint8_t { MissingSeparator, EmptyKey, EmptyValue };

// What went wrong, and the zero-based column where the offending token starts.
struct ParseError {
    ErrorKind kind;
    std::size_t column;

    friend bool operator==(const ParseError&, const ParseError&) = default;
};

[[nodiscard]] constexpr std::string_view describe(ErrorKind kind) noexcept {
    switch (kind) {
    case ErrorKind::MissingSeparator:
        return "missing '=' between key and value";
    case ErrorKind::EmptyKey:
        return "empty key before '='";
    case ErrorKind::EmptyValue:
        return "empty value after '='";
    }
    return "unknown parse error";
}

// Splits a line into blank-separated key=value fields; a blank line has none.
// The key ends at the first '=', so the value may itself contain '='.
[[nodiscard]] constexpr std::expected<std::vector<Field>, ParseError>
parse_line(std::string_view line) {
    constexpr std::string_view blanks = " \t\r";
    std::vector<Field> fields;
    std::size_t start = line.find_first_not_of(blanks);
    while (start != std::string_view::npos) {
        const std::size_t end = std::min(line.find_first_of(blanks, start), line.size());
        const std::string_view token = line.substr(start, end - start);
        const std::size_t separator = token.find('=');
        if (separator == std::string_view::npos) {
            return std::unexpected(ParseError{.kind = ErrorKind::MissingSeparator, .column = start});
        }
        if (separator == 0) {
            return std::unexpected(ParseError{.kind = ErrorKind::EmptyKey, .column = start});
        }
        if (separator + 1 == token.size()) {
            return std::unexpected(ParseError{.kind = ErrorKind::EmptyValue, .column = start});
        }
        fields.push_back(Field{.key = token.substr(0, separator), .value = token.substr(separator + 1)});
        start = line.find_first_not_of(blanks, end);
    }
    return fields;
}

} // namespace myproj::parser
