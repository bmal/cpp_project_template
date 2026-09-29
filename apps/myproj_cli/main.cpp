// Mechanism: an app that wires modules together and keeps its own logic to orchestration.
// Reads key=value lines from stdin, echoes each field, and exits 1 on the first malformed line.
#include <core/build_info.hpp>
#include <core/clock.hpp>
#include <core/counter.hpp>
#include <parser/parser.hpp>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <print>
#include <string>

int main() {
    // std::print needs the Homebrew libc++ on macOS; the system copy lacks it.
    std::println("{}", myproj::core::version_banner());

    const myproj::core::SystemClock clock;
    const myproj::core::Stopwatch stopwatch(clock);
    myproj::core::Counter fields;
    std::int64_t line_number = 0;
    for (std::string line; std::getline(std::cin, line);) {
        ++line_number;
        const auto parsed = myproj::parser::parse_line(line);
        if (!parsed.has_value()) {
            std::println(stderr, "line {}, column {}: {}", line_number, parsed.error().column + 1,
                         myproj::parser::describe(parsed.error().kind));
            return 1;
        }
        for (const auto& field : *parsed) {
            std::println("{}: {}", field.key, field.value);
        }
        fields.add(parsed->size());
    }

    const auto micros = std::chrono::duration_cast<std::chrono::microseconds>(stopwatch.elapsed());
    std::println(stderr, "{} lines, {} fields in {} us", line_number, fields.value(), micros.count());
    return 0;
}
