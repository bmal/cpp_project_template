#include <doctest/doctest.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <functional>
#include <trading/asserts.hpp>

namespace test {

class ProcessRunner {
    int pipe_[2]{};
    pid_t pid_{};
    int status_{};

   public:
    // Executes function in child process, captures its stderr
    explicit ProcessRunner(std::function<void()> child_func) {
        if (pipe(pipe_) == -1) {
            throw std::runtime_error("Failed to create pipe");
        }

        pid_ = fork();
        if (pid_ == -1) {
            close(pipe_[0]);
            close(pipe_[1]);
            throw std::runtime_error("Fork failed");
        }

        if (pid_ == 0) {  // Child
            close(pipe_[0]);
            dup2(pipe_[1], STDERR_FILENO);
            close(pipe_[1]);
            child_func();
            _Exit(EXIT_SUCCESS);  // Should never reach here for assertion tests
        }

        close(pipe_[1]);  // Close write end in parent
        waitpid(pid_, &status_, 0);
    }

    ~ProcessRunner() { close(pipe_[0]); }

    [[nodiscard]] bool terminated_abnormally() const noexcept {
        return WIFSIGNALED(status_) ||
               (WIFEXITED(status_) && WEXITSTATUS(status_) == EXIT_FAILURE);
    }

    // Returns captured stderr output
    [[nodiscard]] std::string stderr_output() const {
        std::string buffer(1024, '\0');
        const ssize_t bytes = read(pipe_[0], buffer.data(), buffer.size() - 1);
        if (bytes > 0) {
            buffer.resize(static_cast<size_t>(bytes));
            return buffer;
        }
        return {};
    }
};

}  // namespace test

TEST_SUITE("Assertion Mechanism") {
    TEST_CASE("ASSERT properly handles failure") {
        test::ProcessRunner process([]() { ASSERT(false, "test message"); });

        const auto output = process.stderr_output();
        CHECK(process.terminated_abnormally());

        // Check essential parts of error message
        CHECK(output.find("ASSERT FAILED") != std::string::npos);
        CHECK(output.find("test message") != std::string::npos);
        CHECK(output.find(__FILE__) != std::string::npos);
    }

    TEST_CASE("FATAL properly terminates") {
        test::ProcessRunner process([]() { FATAL("fatal message"); });

        const auto output = process.stderr_output();
        CHECK(process.terminated_abnormally());
        CHECK(output.find("FATAL: fatal message") != std::string::npos);
    }

// Release mode behavior
#ifdef NDEBUG
    TEST_CASE("Assertions are stripped in release builds") {
        test::ProcessRunner process([]() {
            ASSERT(false, "should be stripped");
            exit(EXIT_SUCCESS);  // Should reach here in release
        });

        CHECK_FALSE(process.terminated_abnormally());
        CHECK(process.output().empty());
    }
#endif

}  // TEST_SUITE
