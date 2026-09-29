// Mechanism: functional tests reach the app only as a real process started from its built path.
// Input comes from a file on stdin and output goes to files, so a full pipe cannot hang a test.
#pragma once

#include <expected>
#include <string>
#include <string_view>

namespace myproj::functional {

struct CliRun {
    int exit_code;
    std::string out;
    std::string err;
};

// Runs myproj_cli with input on stdin and waits for it to exit.
// Fails with a message naming the binary when it cannot be started.
[[nodiscard]] std::expected<CliRun, std::string> run_cli(std::string_view input);

} // namespace myproj::functional
