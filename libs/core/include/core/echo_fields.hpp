// Mechanism: orchestration written against an interface at the one true edge, the program's output.
// Integration tests put a GoogleMock behind OutputSink; the CLI puts the console there.
#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <iosfwd>
#include <string_view>

namespace myproj::core {

// Where echoed fields go. Implementations own formatting and the destination.
class OutputSink {
public:
    OutputSink() = default;
    OutputSink(const OutputSink&) = delete;
    OutputSink& operator=(const OutputSink&) = delete;
    OutputSink(OutputSink&&) = delete;
    OutputSink& operator=(OutputSink&&) = delete;
    virtual ~OutputSink() = default;

    virtual void field(std::string_view key, std::string_view value) = 0;
};

struct EchoTotals {
    std::uint64_t lines;
    std::uint64_t fields;
};

// The first malformed line; line and column count from one.
struct EchoError {
    std::uint64_t line;
    std::size_t column;
    std::string_view message;
};

// Parses every key=value line of input and sends each field to the sink, in
// order. Stops at the first malformed line, after the fields of the lines
// before it.
[[nodiscard]] std::expected<EchoTotals, EchoError> echo_fields(std::istream& input,
                                                               OutputSink& sink);

} // namespace myproj::core
