#include <gtest/gtest.h>
#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include "trading/logger.hpp"

class LoggerTest : public ::testing::Test {
   protected:
    std::filesystem::path _testPath;

    void SetUp() override {
        _testPath = std::filesystem::temp_directory_path() / "testlog.txt";
        std::filesystem::remove(_testPath);
    }

    void TearDown() override { std::filesystem::remove(_testPath); }

    std::string readLogFile() {
        // Wait briefly for logger to process messages
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        std::ifstream file(_testPath);
        if (!file) {
            throw std::runtime_error("Failed to read test file");
        }
        return {(std::istreambuf_iterator<char>(file)),
                std::istreambuf_iterator<char>()};
    }

    // Helper to wait for log file to contain expected content
    bool waitForContent(const std::string& expected,
                        std::chrono::milliseconds timeout) {
        auto start = std::chrono::steady_clock::now();
        while (std::chrono::steady_clock::now() - start < timeout) {
            if (readLogFile() == expected) {
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return false;
    }
};

TEST_F(LoggerTest, WhenLoggingPrimitiveTypesShouldWriteCorrectly) {
    {
        Trading::Core::Logger logger(_testPath);
        logger.enqueueMessage('A');
        logger.enqueueMessage(42);
        logger.enqueueMessage(123.456F);
        logger.enqueueMessage(789.012);
    }  // Wait for destructor to process remaining messages

    EXPECT_TRUE(
        waitForContent("A42123.456789.012", std::chrono::milliseconds(100)));
}

TEST_F(LoggerTest, WhenLoggingStringShouldWriteCorrectly) {
    const std::string testStr = "Hello World";
    {
        Trading::Core::Logger logger(_testPath);
        logger.enqueueMessage(testStr);
    }

    EXPECT_TRUE(waitForContent(testStr, std::chrono::milliseconds(100)));
}

TEST_F(LoggerTest, WhenQueueFullShouldEventuallySucceed) {
    const size_t messagesToFill =
        (Trading::Core::LOG_QUEUE_SIZE / sizeof(Trading::Core::LogElement)) + 1;

    {
        Trading::Core::Logger logger(_testPath);

        // Fill queue and verify we can still write
        for (size_t i = 0; i < messagesToFill; ++i) {
            logger.enqueueMessage(std::to_string(i));
        }
    }

    // Verify some messages were written
    std::string content = readLogFile();
    EXPECT_FALSE(content.empty());
}

TEST_F(LoggerTest, WhenLoggingFormatStringShouldHandleEscapes) {
    {
        Trading::Core::Logger logger(_testPath);
        logger.log("100%% of % tests", 42);
    }

    EXPECT_TRUE(
        waitForContent("100% of 42 tests", std::chrono::milliseconds(100)));
}

TEST_F(LoggerTest, WhenLoggingConcurrentlyShouldPreserveOrder) {
    constexpr size_t msgCount = 100;
    std::string expected;

    {
        Trading::Core::Logger logger(_testPath);
        for (size_t i = 0; i < msgCount; ++i) {
            std::string msg = std::to_string(i) + "\n";
            expected += msg;
            logger.log("%\n", i);
        }
    }

    EXPECT_TRUE(waitForContent(expected, std::chrono::milliseconds(500)));
}

// Death tests in separate suite
class LoggerDeathTest : public LoggerTest {};

TEST_F(LoggerDeathTest, WhenCreatingWithInvalidPathShouldThrow) {
    testing::FLAGS_gtest_death_test_style = "threadsafe";

    EXPECT_EXIT(
        {
            Trading::Core::Logger logger(
                "/invalid/path/that/should/not/exist/test.log");
        },
        ::testing::ExitedWithCode(EXIT_FAILURE),
        "FATAL: Could not open log file");
}

TEST_F(LoggerDeathTest, WhenFormatStringMismatchShouldTerminate) {
    Trading::Core::Logger logger(_testPath);
    EXPECT_DEATH({ logger.log("Value: %"); }, "missing arguments to log");
    EXPECT_DEATH(
        { logger.log("Value", 42); }, "extra arguments provided to log");
}

TEST_F(LoggerTest, WhenDestructingShouldProcessRemainingMessages) {
    std::string expected;
    {
        Trading::Core::Logger logger(_testPath);
        // Write enough messages to potentially fill queue but not overflow
        for (int i = 0; i < 1000; ++i) {
            std::string msg = std::to_string(i) + "\n";
            expected += msg;
            logger.log("%\n", i);
        }
    }  // Destructor should process remaining messages

    EXPECT_TRUE(waitForContent(expected, std::chrono::milliseconds(1000)))
        << "Not all messages were processed before shutdown";
}

TEST_F(LoggerTest, WhenLoggingNumericLimitsShouldPreserveReadability) {
    const int intMax = std::numeric_limits<int>::max();
    const int intMin = std::numeric_limits<int>::min();
    const double doubleVal = 123.456789;

    {
        Trading::Core::Logger logger(_testPath);
        logger.log("% % %", intMax, intMin, doubleVal);
    }

    std::string content = readLogFile();
    std::istringstream iss(content);
    int parsedIntMax;
    int parsedIntMin;
    double parsedDouble;
    iss >> parsedIntMax >> parsedIntMin >> parsedDouble;

    EXPECT_EQ(parsedIntMax, intMax);
    EXPECT_EQ(parsedIntMin, intMin);
    EXPECT_NEAR(parsedDouble, doubleVal, 1e-6);
}
