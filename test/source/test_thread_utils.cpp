#include <doctest/doctest.h>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <exception>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <trading/thread_utils.hpp>
#include <utility>
#include <vector>

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
    if (maxCores == 0) {
        MESSAGE("Skipping test: Unable to determine core count");
        return;
    }

    SUBCASE("Thread operations") {
        // Test cases with atomic to track thread execution
        std::atomic<bool> threadRan{false};

        // Case 1: Unpinned thread (-1)
        {
            auto thread = Trading::Core::createPinnedThread(
                -1, "unpinned", [&]() { threadRan = true; });
            thread.join();
            CHECK(threadRan);
        }

        // Case 2: Valid core (0)
        {
            threadRan = false;
            auto thread = Trading::Core::createPinnedThread(
                0, "pinned", [&]() { threadRan = true; });
            thread.join();
            CHECK(threadRan);
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

// below tests are temporary
TEST_CASE("Thread Safety Diagnostic Test") {
    std::cout << "Starting Thread Safety Test...\n";

    const int NUM_THREADS = 4;
    std::atomic<int> counter{0};
    std::atomic<bool> should_continue{true};
    std::vector<std::thread> threads;
    std::mutex mtx;

    std::cout << "Launching " << NUM_THREADS << " threads...\n";

    // Create threads that will contend for resources
    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back([&, i]() {
            std::cout << "Thread " << i << " started\n";

            while (should_continue) {
                {
                    std::lock_guard<std::mutex> lock(mtx);
                    counter++;

                    // Deliberate race condition if mutex wasn't working
                    if (counter % 100 == 0) {
                        std::cout << "Thread " << i << " counter: " << counter
                                  << "\n";
                    }
                }

                std::this_thread::sleep_for(std::chrono::microseconds(1));
            }

            std::cout << "Thread " << i << " finished\n";
        });
    }

    // Let threads run for a bit
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    should_continue = false;

    // Join all threads
    for (auto& thread : threads) {
        thread.join();
    }

    std::cout << "Final counter value: " << counter << "\n";
}

TEST_CASE("Memory Diagnostic Test") {
    std::cout << "Starting Memory Test...\n";

    const size_t NUM_ALLOCATIONS = 1000;
    const size_t ALLOCATION_SIZE = 1024;  // 1KB
    std::vector<std::unique_ptr<uint8_t[]>> allocations;

    std::cout << "Performing " << NUM_ALLOCATIONS << " allocations of "
              << ALLOCATION_SIZE << " bytes each...\n";

    // Perform many allocations
    for (size_t i = 0; i < NUM_ALLOCATIONS; ++i) {
        allocations.push_back(std::make_unique<uint8_t[]>(ALLOCATION_SIZE));

        // Write to memory to ensure it's actually allocated
        std::memset(allocations.back().get(), i & 0xFF, ALLOCATION_SIZE);

        if (i % 100 == 0) {
            std::cout << "Allocation " << i << " complete\n";
        }
    }

    // Clear half the allocations
    std::cout << "Clearing half of allocations...\n";
    allocations.erase(allocations.begin(),
                      allocations.begin() + (NUM_ALLOCATIONS / 2));

    std::cout << "Memory test complete. Remaining allocations: "
              << allocations.size() << "\n";
}
