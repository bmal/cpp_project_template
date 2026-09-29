// Mechanism: a libFuzzer harness; the fuzzer calls it with inputs it mutates toward new coverage.
// Inputs may be malformed, but every field parse_line returns must keep the parser's promises.
#include "parser/parser.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iterator>
#include <string_view>

// NOLINTNEXTLINE(readability-identifier-naming): libFuzzer calls the harness by this name.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): bytes in, characters out.
    const std::string_view line(reinterpret_cast<const char*>(data), size);

    const auto fields = myproj::parser::parse_line(line);

    // Where a part starts in the line; negative or past the end when it points outside it.
    const auto offset = [line](std::string_view part) { return part.data() - line.data(); };
    if (fields.has_value()) {
        for (const auto& field : *fields) {
            const bool broken = field.key.empty() || field.value.empty() ||
                                field.key.contains('=') || offset(field.key) < 0 ||
                                offset(field.value) + std::ssize(field.value) > std::ssize(line);
            if (broken) {
                std::abort();
            }
        }
    }
    return 0;
}
