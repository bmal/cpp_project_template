// Mechanism: an app that wires modules together and keeps its own logic to orchestration.
// Reads key=value lines from stdin, echoes each field, and exits 1 on the first malformed line.
#include <core/build_info.hpp>
#include <core/clock.hpp>
#include <core/echo_fields.hpp>

#include <chrono>
#include <cstdio>
#include <iostream>
#include <print>
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

} // namespace

// NOLINTNEXTLINE(bugprone-exception-escape): an escaping exception should terminate and dump core.
int main() {
    // std::print needs the Homebrew libc++ on macOS; the system copy lacks it.
    std::println("{}", myproj::core::version_banner());

    const myproj::core::SystemClock clock;
    const myproj::core::Stopwatch stopwatch(clock);
    ConsoleSink sink;
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
