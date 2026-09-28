#include <module/component.hpp>
#include <module/header_only.hpp>

#include <print>

int main() {
    Example::Counter counter;
    counter.increment(41);

    const auto result = Example::safe_divide(counter.increment().get(), 6);
    if (!result.has_value())
        return 1;

    // std::print needs the Homebrew libc++ on macOS; the system copy lacks it.
    std::println("42 / 6 = {}", result.value());
    return result.value() == 7 ? 0 : 1;
}
