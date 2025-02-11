#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <atomic>
#include <cstring>
#include <exception>
#include <functional>
#include <memory>
#include <stdexcept>
#include <thread>
#include <trading/thread_utils.hpp>
#include <utility>

constexpr int NO_AFFINITY_CORE = 0;

std::thread::id getMainThreadId() {
    static const std::thread::id mainThreadId = std::this_thread::get_id();
    return mainThreadId;
}

class ThreadTestBase : public testing::Test {
   protected:
    void waitForThread(std::thread& thread) {
        ASSERT_NO_FATAL_FAILURE(thread.join());
    }
};

class BasicThreadTest : public ThreadTestBase {
   protected:
    void SetUp() override {}
    void TearDown() override {}

    template <typename Func, typename... Args>
    std::thread createTestThread(const std::string& name,
                                 Func&& func,
                                 Args&&... args) {
        return Trading::Core::createPinnedThread(NO_AFFINITY_CORE, name,
                                                 std::forward<Func>(func),
                                                 std::forward<Args>(args)...);
    }
};

TEST_F(BasicThreadTest, ExecutesFunction) {
    std::atomic<bool> wasExecuted{false};
    std::atomic<std::thread::id> threadId;

    auto thread = createTestThread("basicThread", [&]() {
        wasExecuted = true;
        threadId = std::this_thread::get_id();
    });

    waitForThread(thread);
    EXPECT_TRUE(wasExecuted);
    EXPECT_NE(threadId, getMainThreadId());
}

TEST_F(BasicThreadTest, HandlesThrowingFunction) {
    std::atomic<bool> exceptionCaught{false};

    auto thread = createTestThread("throwingThread", [&]() {
        try {
            throw std::runtime_error("test");
        } catch (...) {
            exceptionCaught = true;
        }
    });

    waitForThread(thread);
    EXPECT_TRUE(exceptionCaught);
}

class ThreadArgumentTest : public ThreadTestBase {
   protected:
    static constexpr int EXPECTED_VALUE = 42;
};

TEST_F(ThreadArgumentTest, HandlesValueArguments) {
    std::atomic<int> result{0};

    auto thread = Trading::Core::createPinnedThread(
        NO_AFFINITY_CORE, "valueArg",
        [](std::atomic<int>& val, int arg) { val = arg; }, std::ref(result),
        EXPECTED_VALUE);

    waitForThread(thread);
    EXPECT_EQ(result, EXPECTED_VALUE);
}

TEST_F(ThreadArgumentTest, HandlesReferenceArguments) {
    int value = 0;
    std::atomic<bool> completed{false};

    auto thread = Trading::Core::createPinnedThread(
        NO_AFFINITY_CORE, "refArg",
        [](int& val, std::atomic<bool>& done) {
            val = EXPECTED_VALUE;
            done = true;
        },
        std::ref(value), std::ref(completed));

    waitForThread(thread);
    EXPECT_TRUE(completed);
    EXPECT_EQ(value, EXPECTED_VALUE);
}

TEST_F(ThreadArgumentTest, HandlesMoveOnlyArguments) {
    std::atomic<bool> moveOccurred{false};
    auto uniquePtr = std::make_unique<int>(EXPECTED_VALUE);

    auto thread = Trading::Core::createPinnedThread(
        NO_AFFINITY_CORE, "moveArg",
        [&moveOccurred](std::unique_ptr<int> ptr) {
            moveOccurred = (*ptr == EXPECTED_VALUE);
        },
        std::move(uniquePtr));

    waitForThread(thread);
    EXPECT_TRUE(moveOccurred);
    EXPECT_EQ(uniquePtr, nullptr);
}

#ifdef __linux__
class LinuxThreadAffinityTest : public ThreadTestBase {
   protected:
    void SetUp() override {
        maxCores = std::thread::hardware_concurrency();
        if (maxCores == 0) {
            GTEST_SKIP() << "Unable to determine core count";
        }
    }

    unsigned int maxCores;
};

TEST_F(LinuxThreadAffinityTest, SupportsUnpinnedThread) {
    std::atomic<bool> threadRan{false};

    auto thread = Trading::Core::createPinnedThread(
        -1, "unpinned", [&]() { threadRan = true; });

    waitForThread(thread);
    EXPECT_TRUE(threadRan);
}

TEST_F(LinuxThreadAffinityTest, SupportsPinnedThread) {
    std::atomic<bool> threadRan{false};

    auto thread = Trading::Core::createPinnedThread(
        0, "pinned", [&]() { threadRan = true; });

    waitForThread(thread);
    EXPECT_TRUE(threadRan);
}

#else
TEST_F(BasicThreadTest, NonLinuxHandlesInvalidCoreIdGracefully) {
    const auto maxCores = std::thread::hardware_concurrency();
    const int invalidCoreId = static_cast<int>(maxCores + 1);
    std::atomic<bool> threadStarted{false};

    auto thread = Trading::Core::createPinnedThread(
        invalidCoreId, "invalidCore", [&threadStarted]() {
            threadStarted.store(true, std::memory_order_release);
        });

    waitForThread(thread);
    EXPECT_TRUE(threadStarted.load(std::memory_order_acquire));
    EXPECT_TRUE(Trading::Core::pinThreadToCore(invalidCoreId));
}
#endif
