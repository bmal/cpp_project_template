// Mechanism: a compiled module using a header-only one as a private dependency.
// Wires parser::parse_line and core::Counter; the only effect on the world goes through the sink.
#include "core/echo_fields.hpp"

#include "core/counter.hpp"

#include <parser/parser.hpp>

#include <istream>
#include <string>

namespace myproj::core {

std::expected<EchoTotals, EchoError> echo_fields(std::istream& input, OutputSink& sink) {
    Counter fields;
    std::uint64_t line_number = 0;
    for (std::string line; std::getline(input, line);) {
        ++line_number;
        const auto parsed = parser::parse_line(line);
        if (!parsed.has_value()) {
            return std::unexpected(EchoError{.line = line_number,
                                             .column = parsed.error().column + 1,
                                             .message = parser::describe(parsed.error().kind)});
        }
        for (const auto& field : *parsed) {
            sink.field(field.key, field.value);
        }
        fields.add(parsed->size());
    }
    return EchoTotals{.lines = line_number, .fields = fields.value()};
}

} // namespace myproj::core
