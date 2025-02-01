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
    const auto maxCores = std::thread::hardware_concurrency();

    SUBCASE("Invalid core ID handling") {
        // Skip if we can't determine core count
        if (maxCores == 0) {
            MESSAGE("Skipping test: Unable to determine core count");
            return;
        }

        constexpr int invalidCoreId = 1024;  // Large enough to be invalid
        REQUIRE(static_cast<unsigned int>(invalidCoreId) > maxCores);

        std::atomic<bool> threadStarted{false};

        // Use a try-catch block to properly handle the exception
        try {
            std::thread t = Trading::Core::createPinnedThread(
                invalidCoreId, "invalidCore",
                [&threadStarted]() { threadStarted = true; });

            // If we somehow get here, clean up properly
            if (t.joinable()) {
                t.join();
            }
            // Test should fail if we reach here
            CHECK(false);
        } catch (const std::runtime_error&) {
            // This is the expected case
            CHECK(true);
        }

        CHECK_FALSE(threadStarted);
        CHECK_FALSE(Trading::Core::pinThreadToCore(invalidCoreId));
    }

    SUBCASE("Valid core ID handling") {
        // Skip test if we can't determine core count
        if (maxCores == 0) {
            MESSAGE("Skipping test: Unable to determine core count");
            return;
        }

        constexpr int testCore = 0;
        REQUIRE_MESSAGE(static_cast<unsigned int>(testCore) < maxCores,
                        "Test requires at least one CPU core");

        std::atomic<bool> success{false};
        std::atomic<int> assignedCore{-1};

        try {
            std::thread thread = Trading::Core::createPinnedThread(
                testCore, "coreAffinity", [&]() {
                    // First check if we can pin to the core
                    success = Trading::Core::pinThreadToCore(testCore);
                    if (!success)
                        return;

                    // Then verify the actual core assignment
                    cpu_set_t cpuset;
                    CPU_ZERO(&cpuset);
                    if (pthread_getaffinity_np(
                            pthread_self(), sizeof(cpu_set_t), &cpuset) == 0) {
                        for (int i = 0;
                             i <
                             std::min(CPU_SETSIZE, static_cast<int>(maxCores));
                             i++) {
                            if (CPU_ISSET(i, &cpuset)) {
                                assignedCore = i;
                                break;
                            }
                        }
                    }
                });

            if (thread.joinable()) {
                thread.join();
            }

            CHECK_MESSAGE(success, "Thread should successfully pin to core 0");

            if (success) {
                CHECK_MESSAGE(
                    assignedCore == testCore,
                    "Thread should be assigned to the requested core");
            }
        } catch (const std::exception& e) {
            INFO("Unexpected exception: " << e.what());
            CHECK(false);
        }
    }
}
#else
TEST_CASE("Non-Linux core affinity") {
    const auto maxCores = std::thread::hardware_concurrency();
    const int invalidCoreId = static_cast<int>(maxCores + 1);
    std::atomic<bool> threadStarted{false};

    // Non-Linux should succeed even with invalid core
    auto thread = Trading::Core::createPinnedThread(
        invalidCoreId, "invalidCore", [&threadStarted]() {
            threadStarted.store(true, std::memory_order_release);
        });

    thread.join();

    CHECK(threadStarted.load(std::memory_order_acquire));
    CHECK(Trading::Core::pinThreadToCore(invalidCoreId));
}
#endif
