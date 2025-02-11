#include <gtest/gtest.h>
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
        const auto bytes = read(_pipe[0], buffer.data(), buffer.size() - 1);
        if (bytes > 0) {
            buffer.resize(static_cast<size_t>(bytes));
            return buffer;
        }
        return {};
    }
};

}  // namespace

class AssertionMechanismTest : public testing::Test {
   protected:
    void SetUp() override {}
    void TearDown() override {}
};

#ifndef NDEBUG
TEST_F(AssertionMechanismTest, AssertProperlyHandlesFailureInDebugMode) {
    ProcessRunner process([]() { ASSERT(false, "test message"); });

    const auto output = process.getStderrOutput();
    EXPECT_TRUE(process.isTerminatedAbnormally());

    // Check essential parts of error message
    EXPECT_NE(output.find("ASSERT FAILED"), std::string::npos);
    EXPECT_NE(output.find("test message"), std::string::npos);
    EXPECT_NE(output.find(__FILE__), std::string::npos);
}

TEST_F(AssertionMechanismTest, AssertAllowsExecutionWhenConditionIsTrue) {
    ProcessRunner process([]() {
        ASSERT(true, "should not see this");
        _Exit(EXIT_SUCCESS);
    });

    EXPECT_TRUE(process.isTerminatedNormally());
    EXPECT_TRUE(process.getStderrOutput().empty());
}
#endif

#ifdef NDEBUG
TEST_F(AssertionMechanismTest, AssertIsStrippedInReleaseMode) {
    ProcessRunner process([]() {
        ASSERT(false, "should be stripped");
        _Exit(EXIT_SUCCESS);  // Should reach here in Release
    });

    EXPECT_TRUE(process.isTerminatedNormally());
    EXPECT_TRUE(process.getStderrOutput().empty());
}
#endif

TEST_F(AssertionMechanismTest, FatalProperlyTerminatesInAllBuildModes) {
    ProcessRunner process([]() { FATAL("fatal message"); });

    const auto output = process.getStderrOutput();
    EXPECT_TRUE(process.isTerminatedAbnormally());
    EXPECT_NE(output.find("FATAL: fatal message"), std::string::npos);
}
