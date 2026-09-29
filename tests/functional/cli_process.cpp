// Mechanism: posix_spawn with files on stdin, stdout, and stderr, and nothing shared with the test.
// A signal that kills the app shows as 128 plus the signal number, as a shell reports it.
#include "cli_process.hpp"

#include <fcntl.h>
#include <spawn.h>
// NOLINTNEXTLINE(misc-include-cleaner): glibc defines pid_t here, and macOS in a private header.
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <cstdlib>
#include <expected>
#include <filesystem>
#include <fstream>
#include <ios>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

// unistd.h declares environ only under _GNU_SOURCE, which libstdc++ turns on.
#ifndef _GNU_SOURCE
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): POSIX declares it so.
extern char** environ;
#endif

namespace myproj::functional {

namespace {

constexpr const char* cli_path = MYPROJ_CLI_PATH;

std::string read_file(const std::filesystem::path& path) {
    const std::ifstream file(path, std::ios::binary);
    std::ostringstream text;
    text << file.rdbuf();
    return std::move(text).str();
}

std::string cannot(std::string_view what, int error) {
    return std::string(what) + " " + cli_path + ": " + std::generic_category().message(error);
}

// A directory of its own per run, removed when the run is over.
class ScratchDir {
public:
    ScratchDir() {
        std::string name = (std::filesystem::temp_directory_path() / "myproj_cli.XXXXXX").string();
        if (::mkdtemp(name.data()) != nullptr) {
            path_ = name;
        }
    }
    ScratchDir(const ScratchDir&) = delete;
    ScratchDir& operator=(const ScratchDir&) = delete;
    ScratchDir(ScratchDir&&) = delete;
    ScratchDir& operator=(ScratchDir&&) = delete;
    ~ScratchDir() {
        std::error_code ignored;
        std::filesystem::remove_all(path_, ignored);
    }

    [[nodiscard]] const std::filesystem::path& path() const noexcept { return path_; }

private:
    std::filesystem::path path_;
};

} // namespace

std::expected<CliRun, std::string> run_cli(std::string_view input) {
    const ScratchDir scratch;
    if (scratch.path().empty()) {
        return std::unexpected(cannot("cannot create a scratch directory to run", errno));
    }
    const auto in = scratch.path() / "stdin";
    const auto out = scratch.path() / "stdout";
    const auto err = scratch.path() / "stderr";
    std::ofstream(in, std::ios::binary) << input;

    posix_spawn_file_actions_t actions{};
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_addopen(&actions, 0, in.c_str(), O_RDONLY, 0);
    posix_spawn_file_actions_addopen(&actions, 1, out.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
    posix_spawn_file_actions_addopen(&actions, 2, err.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
    std::string program = cli_path;
    const std::array<char*, 2> argv = {program.data(), nullptr};
    pid_t pid = 0;
    const int spawn_error = posix_spawn(&pid, cli_path, &actions, nullptr, argv.data(), environ);
    posix_spawn_file_actions_destroy(&actions);
    if (spawn_error != 0) {
        return std::unexpected(cannot("cannot start", spawn_error));
    }

    int status = 0;
    while (waitpid(pid, &status, 0) == -1) {
        if (errno != EINTR) {
            return std::unexpected(cannot("cannot wait for", errno));
        }
    }
    const int exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
    return CliRun{.exit_code = exit_code, .out = read_file(out), .err = read_file(err)};
}

} // namespace myproj::functional
