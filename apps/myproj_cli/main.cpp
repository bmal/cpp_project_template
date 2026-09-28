#include <core/component.hpp>
#include <core/header_only.hpp>
#include <myproj/version.hpp>

#include <print>

int main() {
    std::println("myproj {} ({}{})", myproj::version_string, myproj::git_commit,
                 myproj::git_dirty ? "-dirty" : "");

    myproj::Counter counter;
    counter.increment(41);

    const auto result = myproj::safe_divide(counter.increment().get(), 6);
    if (!result.has_value())
        return 1;

    // std::print needs the Homebrew libc++ on macOS; the system copy lacks it.
    std::println("42 / 6 = {}", result.value());
    return result.value() == 7 ? 0 : 1;
}
