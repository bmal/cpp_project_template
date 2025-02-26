#include <benchmark/benchmark.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <queue>
#include <thread>
#include <vector>
#include "trading/spsc_queue.hpp"

namespace {

constexpr std::size_t SMALL_QUEUE_SIZE = 128U;
constexpr std::size_t MEDIUM_QUEUE_SIZE = 1024U;
constexpr std::size_t LARGE_QUEUE_SIZE = 4096U;
constexpr std::size_t WARMUP_ITERATIONS = 1000U;
constexpr std::size_t BATCH_SIZE = 1000U;
constexpr std::size_t BURST_SIZE = 100U;

struct alignas(64) QueueMessage {
    std::uint64_t timestamp;
    double price;
    std::uint32_t quantity;
    std::array<char, 8> symbol;
    std::array<std::byte, 36> padding{};
};
static_assert(sizeof(QueueMessage) == 64, "QueueMessage must be 64 bytes");

class ScopedThread {
    std::thread thread_;

   public:
    template <typename F>
    explicit ScopedThread(F&& f) : thread_(std::forward<F>(f)) {}
    ~ScopedThread() {
        if (thread_.joinable())
            thread_.join();
    }
    ScopedThread(const ScopedThread&) = delete;
    ScopedThread& operator=(const ScopedThread&) = delete;
};

static void baselineQueueLatency(benchmark::State& state) {
    std::queue<QueueMessage> queue;
    QueueMessage msg{};
    std::vector<double> latencies;
    latencies.reserve(static_cast<std::size_t>(state.max_iterations));

    for (std::size_t i = 0; i < WARMUP_ITERATIONS; ++i) {
        queue.push(msg);
        queue.pop();
    }

    for (auto _ : state) {
        auto start = std::chrono::steady_clock::now();
        queue.push(msg);
        queue.pop();
        auto end = std::chrono::steady_clock::now();

        auto elapsed_ns =
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)
                .count();
        latencies.push_back(static_cast<double>(elapsed_ns));
        benchmark::ClobberMemory();
    }

    if (!latencies.empty()) {
        std::sort(latencies.begin(), latencies.end());
        const auto size = latencies.size();
        const auto p50_idx = size / 2;
        const auto p99_idx =
            static_cast<std::size_t>(static_cast<double>(size) * 0.99);
        const auto p999_idx =
            static_cast<std::size_t>(static_cast<double>(size) * 0.999);

        state.counters["p50"] = latencies[p50_idx];
        state.counters["p99"] = latencies[p99_idx];
        state.counters["p99.9"] = latencies[p999_idx];
    }
}

static void queueSingleThreadLatency(benchmark::State& state) {
    const auto queueSize = static_cast<std::size_t>(state.range(0));
    if (queueSize == 0) {
        state.SkipWithError("Queue size cannot be zero");
        return;
    }

    Trading::Core::SPSCQueue<QueueMessage> queue(queueSize);
    QueueMessage msg{};
    std::vector<double> latencies;
    latencies.reserve(static_cast<std::size_t>(state.max_iterations));

    for (std::size_t i = 0; i < WARMUP_ITERATIONS; ++i) {
        [[maybe_unused]] bool warmupPush = queue.tryPush(msg);
        [[maybe_unused]] bool warmupPop = queue.tryPop(msg);
    }

    for (auto _ : state) {
        auto start = std::chrono::steady_clock::now();
        bool pushResult = queue.tryPush(msg);
        bool popResult = queue.tryPop(msg);
        auto end = std::chrono::steady_clock::now();

        if (!pushResult || !popResult) {
            state.SkipWithError("Queue operation failed");
            break;
        }

        auto elapsed_ns =
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)
                .count();
        latencies.push_back(static_cast<double>(elapsed_ns));
        benchmark::ClobberMemory();
    }

    if (!latencies.empty()) {
        std::sort(latencies.begin(), latencies.end());
        const auto size = latencies.size();
        const auto p50_idx = size / 2;
        const auto p99_idx =
            static_cast<std::size_t>(static_cast<double>(size) * 0.99);
        const auto p999_idx =
            static_cast<std::size_t>(static_cast<double>(size) * 0.999);

        state.counters["p50"] = latencies[p50_idx];
        state.counters["p99"] = latencies[p99_idx];
        state.counters["p99.9"] = latencies[p999_idx];
    }
}

static void queueSustainedThroughput(benchmark::State& state) {
    const auto queueSize = static_cast<std::size_t>(state.range(0));
    if (queueSize == 0) {
        state.SkipWithError("Queue size cannot be zero");
        return;
    }

    Trading::Core::SPSCQueue<QueueMessage> queue(queueSize);
    QueueMessage msg{};
    std::atomic<bool> running{true};
    std::atomic<std::uint64_t> messagesProcessed{0};

    ScopedThread consumer([&]() {
        QueueMessage received;
        while (running.load(std::memory_order_relaxed)) {
            if (queue.tryPop(received)) {
                messagesProcessed.fetch_add(1, std::memory_order_relaxed);
            }
        }
    });

    for (std::size_t i = 0; i < WARMUP_ITERATIONS; ++i) {
        [[maybe_unused]] bool warmupPush = queue.tryPush(msg);
    }

    for (auto _ : state) {
        for (std::size_t i = 0; i < BATCH_SIZE; ++i) {
            while (!queue.tryPush(msg)) {
            }
        }
        benchmark::ClobberMemory();
    }

    running.store(false, std::memory_order_relaxed);
    state.counters["msgs_per_sec"] =
        benchmark::Counter(static_cast<double>(messagesProcessed.load()),
                           benchmark::Counter::kIsRate);
}

static void queueBurstHandling(benchmark::State& state) {
    const auto queueSize = static_cast<std::size_t>(state.range(0));
    if (queueSize == 0) {
        state.SkipWithError("Queue size cannot be zero");
        return;
    }

    Trading::Core::SPSCQueue<QueueMessage> queue(queueSize);
    QueueMessage msg{};
    std::vector<double> burstLatencies;
    burstLatencies.reserve(static_cast<std::size_t>(state.max_iterations));

    for (std::size_t i = 0; i < WARMUP_ITERATIONS; ++i) {
        [[maybe_unused]] bool warmupPush = queue.tryPush(msg);
        [[maybe_unused]] bool warmupPop = queue.tryPop(msg);
    }

    for (auto _ : state) {
        auto start = std::chrono::steady_clock::now();

        for (std::size_t i = 0; i < BURST_SIZE; ++i) {
            if (!queue.tryPush(msg)) {
                state.SkipWithError("Burst push failed");
                return;
            }
        }

        for (std::size_t i = 0; i < BURST_SIZE; ++i) {
            if (!queue.tryPop(msg)) {
                state.SkipWithError("Burst pop failed");
                return;
            }
        }

        auto end = std::chrono::steady_clock::now();
        auto elapsed_ns =
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)
                .count();
        burstLatencies.push_back(static_cast<double>(elapsed_ns) /
                                 static_cast<double>(BURST_SIZE));
        benchmark::ClobberMemory();
    }

    if (!burstLatencies.empty()) {
        std::sort(burstLatencies.begin(), burstLatencies.end());
        const auto size = burstLatencies.size();
        const auto p50_idx = size / 2;
        const auto p99_idx =
            static_cast<std::size_t>(static_cast<double>(size) * 0.99);
        const auto p999_idx =
            static_cast<std::size_t>(static_cast<double>(size) * 0.999);

        state.counters["p50_per_msg"] = burstLatencies[p50_idx];
        state.counters["p99_per_msg"] = burstLatencies[p99_idx];
        state.counters["p99.9_per_msg"] = burstLatencies[p999_idx];
    }
}

static void queueNearFull(benchmark::State& state) {
    Trading::Core::SPSCQueue<QueueMessage> queue(SMALL_QUEUE_SIZE);
    QueueMessage msg{};
    std::vector<double> latencies;
    latencies.reserve(static_cast<std::size_t>(state.max_iterations));

    const auto nearCapacity = queue.capacity() - 8U;
    for (std::size_t i = 0U; i < nearCapacity; ++i) {
        if (!queue.tryPush(msg)) {
            state.SkipWithError("Failed to fill queue");
            return;
        }
    }

    for (auto _ : state) {
        auto start = std::chrono::steady_clock::now();
        bool pushResult = queue.tryPush(msg);
        bool popResult = queue.tryPop(msg);
        auto end = std::chrono::steady_clock::now();

        if (popResult) {
            auto elapsed_ns =
                std::chrono::duration_cast<std::chrono::nanoseconds>(end -
                                                                     start)
                    .count();
            latencies.push_back(static_cast<double>(elapsed_ns));
        }
        benchmark::ClobberMemory();
        benchmark::DoNotOptimize(pushResult);
    }

    if (!latencies.empty()) {
        std::sort(latencies.begin(), latencies.end());
        const auto size = latencies.size();
        const auto p50_idx = size / 2;
        const auto p99_idx =
            static_cast<std::size_t>(static_cast<double>(size) * 0.99);
        const auto p999_idx =
            static_cast<std::size_t>(static_cast<double>(size) * 0.999);

        state.counters["p50"] = latencies[p50_idx];
        state.counters["p99"] = latencies[p99_idx];
        state.counters["p99.9"] = latencies[p999_idx];
        state.counters["success_rate"] =
            static_cast<double>(latencies.size()) /
            static_cast<double>(state.iterations()) * 100.0;
    }
}

}  // namespace

BENCHMARK(baselineQueueLatency)->UseRealTime()->Unit(benchmark::kNanosecond);

BENCHMARK(queueSingleThreadLatency)
    ->Arg(SMALL_QUEUE_SIZE)
    ->Arg(MEDIUM_QUEUE_SIZE)
    ->Arg(LARGE_QUEUE_SIZE)
    ->UseRealTime()
    ->Unit(benchmark::kNanosecond);

BENCHMARK(queueSustainedThroughput)
    ->Arg(SMALL_QUEUE_SIZE)
    ->Arg(MEDIUM_QUEUE_SIZE)
    ->Arg(LARGE_QUEUE_SIZE)
    ->UseRealTime()
    ->Unit(benchmark::kMicrosecond)
    ->Threads(1);

BENCHMARK(queueBurstHandling)
    ->Arg(SMALL_QUEUE_SIZE)
    ->Arg(MEDIUM_QUEUE_SIZE)
    ->UseRealTime()
    ->Unit(benchmark::kMicrosecond);

BENCHMARK(queueNearFull)->UseRealTime()->Unit(benchmark::kNanosecond);
