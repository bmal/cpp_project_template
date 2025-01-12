#include <doctest/doctest.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <functional>
#include <stdexcept>
#include <trading/asserts.hpp>

namespace processTest {

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
            _Exit(EXIT_SUCCESS);  // Should never reach here for assertion tests
        }

        close(_pipe[1]);  // Close write end in parent
        waitpid(_pid, &_status, 0);
    }

    ~ProcessRunner() { close(_pipe[0]); }

    [[nodiscard]] bool isTerminatedAbnormally() const noexcept {
        return WIFSIGNALED(_status) ||
               (WIFEXITED(_status) && WEXITSTATUS(_status) == EXIT_FAILURE);
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

}  // namespace processTest

TEST_SUITE("Assertion Mechanism") {
    TEST_CASE("ASSERT properly handles failure") {
        processTest::ProcessRunner process(
            []() { ASSERT(false, "test message"); });

        const auto output = process.getStderrOutput();
        CHECK(process.isTerminatedAbnormally());

        // Check essential parts of error message
        CHECK(output.find("ASSERT FAILED") != std::string::npos);
        CHECK(output.find("test message") != std::string::npos);
        CHECK(output.find(__FILE__) != std::string::npos);
    }

    TEST_CASE("FATAL properly terminates") {
        processTest::ProcessRunner process([]() { FATAL("fatal message"); });

        const auto output = process.getStderrOutput();
        CHECK(process.isTerminatedAbnormally());
        CHECK(output.find("FATAL: fatal message") != std::string::npos);
    }

#ifdef NDEBUG
    TEST_CASE("Assertions are stripped in release builds") {
        processTest::ProcessRunner process([]() {
            ASSERT(false, "should be stripped");
            exit(EXIT_SUCCESS);  // Should reach here in release
        });

        CHECK_FALSE(process.isTerminatedAbnormally());
        CHECK(process.getStderrOutput().empty());
    }
#endif

}  // TEST_SUITE
