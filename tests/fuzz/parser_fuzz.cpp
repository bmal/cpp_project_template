// Mechanism: a libFuzzer harness; the fuzzer calls it with inputs it mutates toward new coverage.
// Any input may be malformed, but every field parse_line returns must hold what the parser promises.
#include "parser/parser.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string_view>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): bytes in, characters out.
    const std::string_view line(reinterpret_cast<const char*>(data), size);

    const auto fields = myproj::parser::parse_line(line);

    if (fields.has_value()) {
        for (const auto& field : *fields) {
            const bool broken = field.key.empty() || field.value.empty() ||
                                field.key.contains('=') || field.key.data() < line.data() ||
                                field.value.data() + field.value.size() > line.data() + line.size();
            if (broken) {
                std::abort();
            }
        }
    }
    return 0;
}
