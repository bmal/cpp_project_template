// Mechanism: an app that wires modules together and keeps its own logic to orchestration.
// Echoes the key=value fields of its arguments, then of stdin lines; exits 2 or 1 if malformed.
#include <core/build_info.hpp>
#include <core/clock.hpp>
#include <core/echo_fields.hpp>
#include <parser/parser.hpp>

#include <chrono>
#include <cstddef>
#include <cstdio>
#include <iostream>
#include <print>
#include <span>
#include <string_view>

namespace {

// The console end of the output edge: one "key: value" line per field on
// stdout.
class ConsoleSink final : public myproj::core::OutputSink {
public:
    void field(std::string_view key, std::string_view value) override {
        std::println("{}: {}", key, value);
    }
};

// Echoes the fields of each argument through the sink; false after naming the first malformed one.
bool echo_arguments(std::span<char* const> args, myproj::core::OutputSink& sink) {
    for (std::size_t i = 0; i < args.size(); ++i) {
        const auto fields = myproj::parser::parse_line(args[i]);
        if (!fields.has_value()) {
            std::println(stderr, "argument {}: {}", i + 1,
                         myproj::parser::describe(fields.error().kind));
            return false;
        }
        for (const auto& field : *fields) {
            sink.field(field.key, field.value);
        }
    }
    return true;
}

} // namespace

// NOLINTNEXTLINE(bugprone-exception-escape): an escaping exception should terminate and dump core.
int main(int argc, char* argv[]) {
    // std::print needs the Homebrew libc++ on macOS; the system copy lacks it.
    std::println("{}", myproj::core::version_banner());

    ConsoleSink sink;
    if (!echo_arguments(std::span(argv, static_cast<std::size_t>(argc)).subspan(1), sink)) {
        return 2;
    }
    const myproj::core::SystemClock clock;
    const myproj::core::Stopwatch stopwatch(clock);
    const auto totals = myproj::core::echo_fields(std::cin, sink);
    if (!totals.has_value()) {
        std::println(stderr, "line {}, column {}: {}", totals.error().line, totals.error().column,
                     totals.error().message);
        return 1;
    }

    const auto micros = std::chrono::duration_cast<std::chrono::microseconds>(stopwatch.elapsed());
    std::println(stderr, "{} lines, {} fields in {} us", totals->lines, totals->fields,
                 micros.count());
    return 0;
}
