#include <doctest/doctest.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <cstdlib>
#include <functional>
#include <stdexcept>
#include <string>
#include <trading/asserts.hpp>

namespace {

class ProcessRunner {
   private:
    int _pipe[2]{};
    pid_t _pid{};
    int _status{};

   public:
    explicit ProcessRunner(const std::function<void()>& childFunc) {
        if (pipe(_pipe) == -1) {
            throw std::runtime_error("Failed to create pipe");
        }

        _pid = fork();
        if (_pid == -1) {
            close(_pipe[0]);
            close(_pipe[1]);
            throw std::runtime_error("Fork failed");
        }

        if (_pid == 0) {  // Child
            close(_pipe[0]);
            dup2(_pipe[1], STDERR_FILENO);
            close(_pipe[1]);
            childFunc();
            _Exit(
                EXIT_SUCCESS);  // Should only reach here in release mode tests
        }

        close(_pipe[1]);  // Close write end in parent
        waitpid(_pid, &_status, 0);
    }

    ~ProcessRunner() { close(_pipe[0]); }

    [[nodiscard]] bool isTerminatedAbnormally() const noexcept {
        return WIFSIGNALED(_status) ||
               (WIFEXITED(_status) && WEXITSTATUS(_status) == EXIT_FAILURE);
    }

    [[nodiscard]] bool isTerminatedNormally() const noexcept {
        return WIFEXITED(_status) && WEXITSTATUS(_status) == EXIT_SUCCESS;
    }

    [[nodiscard]] std::string getStderrOutput() const {
        std::string buffer(1024, '\0');
        const ssize_t bytes = read(_pipe[0], buffer.data(), buffer.size() - 1);
        if (bytes > 0) {
            buffer.resize(static_cast<size_t>(bytes));
            return buffer;
        }
        return {};
    }
};

}  // namespace

TEST_SUITE("Assertion Mechanism") {
    // Test assertion behavior in Debug mode
#ifndef NDEBUG
    TEST_CASE("ASSERT properly handles failure in Debug mode") {
        ProcessRunner process([]() { ASSERT(false, "test message"); });

        const auto output = process.getStderrOutput();
        CHECK(process.isTerminatedAbnormally());

        // Check essential parts of error message
        CHECK(output.find("ASSERT FAILED") != std::string::npos);
        CHECK(output.find("test message") != std::string::npos);
        CHECK(output.find(__FILE__) != std::string::npos);
    }

    TEST_CASE(
        "ASSERT allows execution to continue when condition is true in Debug "
        "mode") {
        ProcessRunner process([]() {
            ASSERT(true, "should not see this");
            _Exit(EXIT_SUCCESS);
        });

        CHECK(process.isTerminatedNormally());
        CHECK(process.getStderrOutput().empty());
    }
#endif

    // Test assertion behavior in Release mode
#ifdef NDEBUG
    TEST_CASE("ASSERT is stripped in Release mode") {
        ProcessRunner process([]() {
            ASSERT(false, "should be stripped");
            _Exit(EXIT_SUCCESS);  // Should reach here in Release
        });

        CHECK(process.isTerminatedNormally());
        CHECK(process.getStderrOutput().empty());
    }
#endif

    // Test FATAL behavior (should work the same in both Debug and Release)
    TEST_CASE("FATAL properly terminates in all build modes") {
        ProcessRunner process([]() { FATAL("fatal message"); });

        const auto output = process.getStderrOutput();
        CHECK(process.isTerminatedAbnormally());
        CHECK(output.find("FATAL: fatal message") != std::string::npos);
    }
}
