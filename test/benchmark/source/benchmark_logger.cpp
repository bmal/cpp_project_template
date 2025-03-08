#include <benchmark/benchmark.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>
#include "trading/logger.hpp"

namespace {

constexpr std::size_t SMALL_MSG_SIZE = 32U;
constexpr std::size_t MEDIUM_MSG_SIZE = 128U;
constexpr std::size_t LARGE_MSG_SIZE = 512U;
constexpr std::size_t WARMUP_ITERATIONS = 1000U;
constexpr std::size_t BATCH_SIZE = 1000U;
constexpr std::size_t BURST_SIZE = 100U;
constexpr std::size_t CRITICAL_OPS_COUNT = 1000U;

using namespace Trading::Core;

std::string generateMessage(std::size_t size) {
    std::string msg;
    msg.reserve(size);
    msg = "[12345678.123456] value=";
    while (msg.size() < size - 20) {
        msg += "1234567890";
    }
    msg.resize(size - 20);
    return msg;
}

void baselineLoggerLatency(benchmark::State& state) {
    Logger logger("baseline.log");
    const std::string baseMsg = generateMessage(SMALL_MSG_SIZE);
    std::vector<double> latencies;
    latencies.reserve(static_cast<std::size_t>(state.iterations()));

    for (std::size_t i = 0; i < WARMUP_ITERATIONS; ++i) {
        logger.log("%s counter=%zu\n", baseMsg.c_str(), i);
    }

    for (auto _ : state) {
        const auto start = std::chrono::steady_clock::now();
        logger.log("%s counter=%zu\n", baseMsg.c_str(), state.iterations());
        const auto end = std::chrono::steady_clock::now();

        const auto elapsedNs =
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)
                .count();
        latencies.push_back(static_cast<double>(elapsedNs));
        benchmark::ClobberMemory();
    }

    if (!latencies.empty()) {
        std::ranges::sort(latencies);
        const auto size = latencies.size();
        const auto p50Idx = size / 2;
        const auto p99Idx =
            static_cast<std::size_t>(static_cast<double>(size) * 0.99);
        const auto p999Idx =
            static_cast<std::size_t>(static_cast<double>(size) * 0.999);

        state.counters["p50"] = latencies[p50Idx];
        state.counters["p99"] = latencies[p99Idx];
        state.counters["p99.9"] = latencies[p999Idx];
    }
}

void loggerMessageSizeLatency(benchmark::State& state) {
    const auto msgSize = static_cast<std::size_t>(state.range(0));
    Logger logger("size_test.log");
    const std::string baseMsg = generateMessage(msgSize);
    std::vector<double> latencies;
    latencies.reserve(static_cast<std::size_t>(state.iterations()));

    for (std::size_t i = 0; i < WARMUP_ITERATIONS; ++i) {
        logger.log("%s counter=%zu\n", baseMsg.c_str(), i);
    }

    for (auto _ : state) {
        const auto start = std::chrono::steady_clock::now();
        logger.log("%s counter=%zu\n", baseMsg.c_str(), state.iterations());
        const auto end = std::chrono::steady_clock::now();

        const auto elapsedNs =
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)
                .count();
        latencies.push_back(static_cast<double>(elapsedNs));
        benchmark::ClobberMemory();
    }

    if (!latencies.empty()) {
        std::ranges::sort(latencies);
        const auto size = latencies.size();
        const auto p50Idx = size / 2;
        const auto p99Idx =
            static_cast<std::size_t>(static_cast<double>(size) * 0.99);
        const auto p999Idx =
            static_cast<std::size_t>(static_cast<double>(size) * 0.999);

        state.counters["p50"] = latencies[p50Idx];
        state.counters["p99"] = latencies[p99Idx];
        state.counters["p99.9"] = latencies[p999Idx];
    }
}

void loggerSustainedThroughput(benchmark::State& state) {
    Logger logger("throughput.log");
    const std::string baseMsg = generateMessage(SMALL_MSG_SIZE);
    std::atomic<std::uint64_t> messagesProcessed{0};

    // Warmup
    for (std::size_t i = 0; i < WARMUP_ITERATIONS; ++i) {
        logger.log("%s counter=%zu\n", baseMsg.c_str(), i);
    }

    for (auto _ : state) {
        for (std::size_t i = 0; i < BATCH_SIZE; ++i) {
            logger.log("%s counter=%zu\n", baseMsg.c_str(), i);
        }
        messagesProcessed.fetch_add(BATCH_SIZE, std::memory_order_relaxed);
        benchmark::ClobberMemory();
    }

    state.counters["msgs_per_sec"] =
        benchmark::Counter(static_cast<double>(messagesProcessed.load()),
                           benchmark::Counter::kIsRate);
}

void loggerBurstHandling(benchmark::State& state) {
    Logger logger("burst.log");
    const std::string baseMsg = generateMessage(SMALL_MSG_SIZE);
    std::vector<double> burstLatencies;
    burstLatencies.reserve(static_cast<std::size_t>(state.iterations()));

    for (std::size_t i = 0; i < WARMUP_ITERATIONS; ++i) {
        logger.log("%s counter=%zu\n", baseMsg.c_str(), i);
    }

    for (auto _ : state) {
        const auto start = std::chrono::steady_clock::now();

        for (std::size_t i = 0; i < BURST_SIZE; ++i) {
            logger.log("%s counter=%zu\n", baseMsg.c_str(), i);
        }

        const auto end = std::chrono::steady_clock::now();
        const auto elapsedNs =
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)
                .count();
        burstLatencies.push_back(static_cast<double>(elapsedNs) /
                                 static_cast<double>(BURST_SIZE));
        benchmark::ClobberMemory();
    }

    if (!burstLatencies.empty()) {
        std::ranges::sort(burstLatencies);
        const auto size = burstLatencies.size();
        const auto p50Idx = size / 2;
        const auto p99Idx =
            static_cast<std::size_t>(static_cast<double>(size) * 0.99);
        const auto p999Idx =
            static_cast<std::size_t>(static_cast<double>(size) * 0.999);

        state.counters["p50_per_msg"] = burstLatencies[p50Idx];
        state.counters["p99_per_msg"] = burstLatencies[p99Idx];
        state.counters["p99.9_per_msg"] = burstLatencies[p999Idx];
    }
}

void criticalPathWithoutLogging(benchmark::State& state) {
    auto criticalOperation = [](double value) -> double { return value * 2.0; };

    std::vector<double> latencies;
    latencies.reserve(static_cast<std::size_t>(state.iterations()));

    for (std::size_t i = 0; i < WARMUP_ITERATIONS; ++i) {
        double result = criticalOperation(static_cast<double>(i));
        benchmark::DoNotOptimize(&result);
    }

    for (auto _ : state) {
        const auto start = std::chrono::steady_clock::now();

        for (std::size_t i = 0; i < CRITICAL_OPS_COUNT; ++i) {
            double result = criticalOperation(static_cast<double>(i));
            benchmark::DoNotOptimize(&result);
        }

        const auto end = std::chrono::steady_clock::now();

        const auto elapsedNs =
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)
                .count();
        latencies.push_back(static_cast<double>(elapsedNs) /
                            static_cast<double>(CRITICAL_OPS_COUNT));
        benchmark::ClobberMemory();
    }

    if (!latencies.empty()) {
        std::ranges::sort(latencies);
        const auto size = latencies.size();
        const auto p50Idx = size / 2;
        const auto p99Idx =
            static_cast<std::size_t>(static_cast<double>(size) * 0.99);
        const auto p999Idx =
            static_cast<std::size_t>(static_cast<double>(size) * 0.999);

        state.counters["p50"] = latencies[p50Idx];
        state.counters["p99"] = latencies[p99Idx];
        state.counters["p99.9"] = latencies[p999Idx];
    }
}

void criticalPathWithLogging(benchmark::State& state) {
    Logger logger("critical_path.log");

    // Static format string to avoid repeated parsing and string construction
    static constexpr char format[] = "[12345678.123456] value=%.6f\n";

    auto criticalOperation = [](double value) -> double { return value * 2.0; };

    std::vector<double> latencies;
    latencies.reserve(static_cast<std::size_t>(state.iterations()));

    // Warmup with pause to ensure queue drains
    for (std::size_t i = 0; i < WARMUP_ITERATIONS; ++i) {
        double result = criticalOperation(static_cast<double>(i));
        benchmark::DoNotOptimize(&result);
        logger.log(format, result);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    for (auto _ : state) {
        // Perform critical operation first
        double result =
            criticalOperation(static_cast<double>(state.iterations()));
        benchmark::DoNotOptimize(&result);

        // Measure only the logging operation
        const auto start = std::chrono::steady_clock::now();
        logger.log(format, result);
        const auto end = std::chrono::steady_clock::now();

        const auto elapsedNs =
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)
                .count();
        latencies.push_back(static_cast<double>(elapsedNs));
        benchmark::ClobberMemory();
    }

    if (!latencies.empty()) {
        std::ranges::sort(latencies);
        const auto size = latencies.size();
        const auto p50Idx = size / 2;
        const auto p99Idx =
            static_cast<std::size_t>(static_cast<double>(size) * 0.99);
        const auto p999Idx =
            static_cast<std::size_t>(static_cast<double>(size) * 0.999);

        state.counters["p50"] = latencies[p50Idx];
        state.counters["p99"] = latencies[p99Idx];
        state.counters["p99.9"] = latencies[p999Idx];
    }
}

}  // namespace

BENCHMARK(baselineLoggerLatency)
    ->UseRealTime()
    ->Unit(benchmark::kNanosecond)
    ->DisplayAggregatesOnly();

BENCHMARK(loggerMessageSizeLatency)
    ->Arg(static_cast<std::int64_t>(SMALL_MSG_SIZE))
    ->Arg(static_cast<std::int64_t>(MEDIUM_MSG_SIZE))
    ->Arg(static_cast<std::int64_t>(LARGE_MSG_SIZE))
    ->UseRealTime()
    ->Unit(benchmark::kNanosecond)
    ->DisplayAggregatesOnly();

BENCHMARK(loggerSustainedThroughput)
    ->UseRealTime()
    ->Unit(benchmark::kMicrosecond)
    ->MinTime(2.0)
    ->DisplayAggregatesOnly();

BENCHMARK(loggerBurstHandling)
    ->UseRealTime()
    ->Unit(benchmark::kMicrosecond)
    ->DisplayAggregatesOnly();

BENCHMARK(criticalPathWithoutLogging)
    ->UseRealTime()
    ->Unit(benchmark::kNanosecond)
    ->DisplayAggregatesOnly();

BENCHMARK(criticalPathWithLogging)
    ->UseRealTime()
    ->Unit(benchmark::kNanosecond)
    ->DisplayAggregatesOnly();
