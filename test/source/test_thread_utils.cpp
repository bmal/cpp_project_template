#include <doctest/doctest.h>
#include <algorithm>
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
    const int NUM_THREADS = 4;
    const int NUM_ITERATIONS = 1000;

    // Protected shared resources
    std::atomic<int> counter{0};
    std::atomic<bool> should_continue{true};
    std::mutex mtx;
    std::vector<std::string> log_messages;

    // Thread storage
    std::vector<std::thread> threads;
    threads.reserve(NUM_THREADS);

    // Create threads with proper synchronization
    for (int i = 0; i < NUM_THREADS; ++i) {
        threads.emplace_back([&, i]() {
            int local_counter = 0;

            while (should_continue.load(std::memory_order_acquire) &&
                   local_counter < NUM_ITERATIONS) {
                {
                    std::lock_guard<std::mutex> lock(mtx);
                    counter.fetch_add(1, std::memory_order_relaxed);
                    local_counter++;

                    if (local_counter % 100 == 0) {
                        log_messages.push_back("Thread " + std::to_string(i) +
                                               " reached " +
                                               std::to_string(local_counter));
                    }
                }

                // Prevent tight loop, allow other threads to run
                std::this_thread::yield();
            }
        });
    }

    // Let threads run for a fixed duration
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    should_continue.store(false, std::memory_order_release);

    // Clean shutdown
    for (auto& thread : threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }

    // Verify results
    CHECK(counter.load(std::memory_order_relaxed) <=
          NUM_THREADS * NUM_ITERATIONS);
    CHECK(!threads.empty());
    CHECK(!log_messages.empty());
}

TEST_CASE("Memory Diagnostic Test") {
    const size_t NUM_ALLOCATIONS = 100;
    const size_t ALLOCATION_SIZE = 1024;  // 1KB

    // Use RAII containers
    std::vector<std::unique_ptr<std::vector<uint8_t>>> allocations;
    allocations.reserve(NUM_ALLOCATIONS);

    // Controlled allocation
    for (size_t i = 0; i < NUM_ALLOCATIONS; ++i) {
        // Use make_unique for exception safety
        auto data = std::make_unique<std::vector<uint8_t>>();
        data->resize(ALLOCATION_SIZE, static_cast<uint8_t>(i & 0xFF));

        // Verify allocation
        CHECK(data->size() == ALLOCATION_SIZE);
        CHECK((*data)[0] == static_cast<uint8_t>(i & 0xFF));

        allocations.push_back(std::move(data));
    }

    // Verify all allocations
    CHECK(allocations.size() == NUM_ALLOCATIONS);

    // Controlled deallocation
    for (size_t i = 0; i < allocations.size(); i += 2) {
        allocations[i].reset();  // Explicit cleanup of every other allocation
    }

    // Partial cleanup verification
    CHECK(allocations.size() == NUM_ALLOCATIONS);
    auto null_count = static_cast<size_t>(
        std::count_if(allocations.begin(), allocations.end(),
                      [](const auto& ptr) { return ptr == nullptr; }));
    CHECK(null_count == (NUM_ALLOCATIONS + 1) / 2);

    // Vector will clean up remaining allocations automatically
}
