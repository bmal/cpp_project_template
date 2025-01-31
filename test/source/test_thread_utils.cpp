#include <doctest/doctest.h>
#include <atomic>
#include <functional>
#include <memory>
#include <stdexcept>
#include <thread>
#include <trading/thread_utils.hpp>
#include <utility>

namespace {
constexpr int NO_AFFINITY_CORE = 0;

std::thread::id getMainThreadId() {
    static const std::thread::id mainThreadId = std::this_thread::get_id();
    return mainThreadId;
}
}  // namespace

TEST_CASE("Basic thread creation and execution") {
    SUBCASE("Thread executes function") {
        std::atomic<bool> wasExecuted{false};
        std::atomic<std::thread::id> threadId;

        auto thread = Trading::Core::createPinnedThread(
            NO_AFFINITY_CORE, "basicThread", [&]() {
                wasExecuted = true;
                threadId = std::this_thread::get_id();
            });

        thread.join();
        CHECK(wasExecuted);
        CHECK(threadId != getMainThreadId());
    }

    SUBCASE("Thread handles throwing function") {
        std::atomic<bool> exceptionCaught{false};

        auto thread = Trading::Core::createPinnedThread(
            NO_AFFINITY_CORE, "throwingThread", [&]() {
                try {
                    throw std::runtime_error("test");
                } catch (...) {
                    exceptionCaught = true;
                }
            });

        thread.join();
        CHECK(exceptionCaught);
    }
}

TEST_CASE("Thread creation with arguments") {
    SUBCASE("Value arguments") {
        const int expectedValue = 42;
        std::atomic<int> result{0};

        auto thread = Trading::Core::createPinnedThread(
            NO_AFFINITY_CORE, "valueArg",
            [](std::atomic<int>& val, int arg) { val = arg; }, std::ref(result),
            expectedValue);

        thread.join();
        CHECK(result == expectedValue);
    }

    SUBCASE("Reference arguments") {
        const int expectedValue = 42;
        int value = 0;
        std::atomic<bool> completed{false};

        auto thread = Trading::Core::createPinnedThread(
            NO_AFFINITY_CORE, "refArg",
            [](int& val, std::atomic<bool>& done) {
                val = expectedValue;
                done = true;
            },
            std::ref(value), std::ref(completed));

        thread.join();
        CHECK(completed);
        CHECK(value == expectedValue);
    }

    SUBCASE("Move-only type argument") {
        const int expectedValue = 42;
        std::atomic<bool> moveOccurred{false};
        auto uniquePtr = std::make_unique<int>(expectedValue);

        auto thread = Trading::Core::createPinnedThread(
            NO_AFFINITY_CORE, "moveArg",
            [&moveOccurred](std::unique_ptr<int> ptr) {
                moveOccurred = (*ptr == expectedValue);
            },
            std::move(uniquePtr));

        thread.join();
        CHECK(moveOccurred);
        CHECK(uniquePtr == nullptr);
    }
}

#ifdef __linux__
TEST_CASE("Linux specific core affinity") {
    SUBCASE("Invalid core ID handling") {
        const auto maxCores = std::thread::hardware_concurrency();
        const int invalidCoreId = static_cast<int>(maxCores + 1);
        std::atomic<bool> threadStarted{false};

        // Test invalid core ID
        CHECK_THROWS_AS(Trading::Core::createPinnedThread(
                            invalidCoreId, "invalidCore",
                            [&threadStarted]() { threadStarted = true; })
                            .join(),
                        std::runtime_error);

        CHECK_FALSE(threadStarted);
        CHECK_FALSE(Trading::Core::pinThreadToCore(invalidCoreId));
    }

    SUBCASE("Valid core ID handling") {
        const auto maxCores = std::thread::hardware_concurrency();
        if (maxCores > 1) {
            const int testCore = 0;
            std::atomic<bool> coreAffinitySet{false};
            std::atomic<int> assignedCore{-1};

            REQUIRE_NOTHROW([&]() {
                auto thread = Trading::Core::createPinnedThread(
                    testCore, "coreAffinity", [&]() {
                        coreAffinitySet =
                            Trading::Core::pinThreadToCore(testCore);
                        cpu_set_t cpuset;
                        CPU_ZERO(&cpuset);
                        if (pthread_getaffinity_np(pthread_self(),
                                                   sizeof(cpu_set_t),
                                                   &cpuset) == 0) {
                            for (int i = 0; i < CPU_SETSIZE; i++) {
                                if (CPU_ISSET(i, &cpuset)) {
                                    assignedCore = i;
                                    break;
                                }
                            }
                        }
                    });
                thread.join();
            }());

            CHECK(coreAffinitySet);
            CHECK(assignedCore == testCore);
        }
    }
}
#else
TEST_CASE("Non-Linux core affinity") {
    SUBCASE("Thread creation with invalid core") {
        const auto maxCores = std::thread::hardware_concurrency();
        const int invalidCoreId = static_cast<int>(maxCores + 1);
        std::atomic<bool> threadStarted{false};

        REQUIRE_NOTHROW([&]() {
            auto thread = Trading::Core::createPinnedThread(
                invalidCoreId, "invalidCore",
                [&threadStarted]() { threadStarted = true; });
            thread.join();
        }());

        CHECK(threadStarted);
        CHECK(Trading::Core::pinThreadToCore(invalidCoreId));
    }
}
#endif
