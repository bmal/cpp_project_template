// Mechanism: a test data builder for parser input, so a test lists fields instead of spelling a line.
// Starts from a blank line and ends in build(); it needs no module to link.
#pragma once

#include <string>
#include <string_view>

namespace myproj::test_support {

// An input line for parser::parse_line: key=value fields separated by one blank.
class LineBuilder {
public:
    LineBuilder& field(std::string_view key, std::string_view value) {
        line_ += line_.empty() ? "" : " ";
        line_.append(key).append("=").append(value);
        return *this;
    }

    [[nodiscard]] std::string build() const { return line_; }

private:
    std::string line_;
};

} // namespace myproj::test_support
