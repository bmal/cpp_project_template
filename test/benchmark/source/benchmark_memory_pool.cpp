#include <benchmark/benchmark.h>
#include <array>
#include <cstddef>
#include <cstdint>
#include <random>
#include <vector>

#include "trading/memory_pool.hpp"

constexpr std::size_t SMALL_OBJECT_SIZE = 32U;
constexpr std::size_t MEDIUM_OBJECT_SIZE = 256U;
constexpr std::size_t LARGE_OBJECT_SIZE = 1024U;
constexpr std::size_t DEFAULT_POOL_SIZE = 1024U;
constexpr std::size_t MIXED_POOL_SIZE = 1000U;
constexpr std::size_t ALLOCATION_PROBABILITY = 70U;

struct alignas(64) PoolObject {
    explicit PoolObject(int v = 0) : value(v) {}
    int value;
    std::array<std::byte, 60> padding{};  // Padding to make it 64 bytes
};

[[nodiscard]] static std::int64_t calculateBytesProcessed(
    std::uint64_t size,
    std::int64_t iterations) noexcept {
    return static_cast<std::int64_t>(size * 2U *
                                     static_cast<std::uint64_t>(iterations));
}

[[nodiscard]] static std::int64_t calculateItemsProcessed(
    std::uint64_t size,
    std::int64_t iterations) noexcept {
    return static_cast<std::int64_t>(size *
                                     static_cast<std::uint64_t>(iterations));
}

static void baselineNewDelete(benchmark::State& state) {
    const auto objectSize = static_cast<std::size_t>(state.range(0));

    for (auto _ : state) {
        auto* obj = new PoolObject();
        benchmark::DoNotOptimize(obj);
        delete obj;
        benchmark::ClobberMemory();
    }

    state.SetBytesProcessed(
        calculateBytesProcessed(objectSize, state.iterations()));
    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()));
}

static void poolAllocDeallocCycle(benchmark::State& state) {
    const auto objectSize = static_cast<std::size_t>(state.range(0));
    Trading::Core::MemoryPool<PoolObject> memPool(DEFAULT_POOL_SIZE);

    for (auto _ : state) {
        auto* obj = memPool.allocate(42);
        benchmark::DoNotOptimize(obj);
        memPool.deallocate(obj);
        benchmark::ClobberMemory();
    }

    state.SetBytesProcessed(
        calculateBytesProcessed(objectSize, state.iterations()));
    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()));
}

static void poolSequentialBatch(benchmark::State& state) {
    const auto batchSize = static_cast<std::size_t>(state.range(0));
    Trading::Core::MemoryPool<PoolObject> memPool(batchSize * 2U);
    std::vector<PoolObject*> pointers;
    pointers.reserve(batchSize);

    for (auto _ : state) {
        state.PauseTiming();
        pointers.clear();
        state.ResumeTiming();

        for (std::size_t i = 0U; i < batchSize; ++i) {
            pointers.push_back(memPool.allocate(static_cast<int>(i)));
        }
        benchmark::ClobberMemory();

        for (auto* ptr : pointers) {
            memPool.deallocate(ptr);
        }
        benchmark::ClobberMemory();
    }

    const auto bytesPerIteration =
        static_cast<std::uint64_t>(batchSize) * sizeof(PoolObject) * 2U;
    state.SetBytesProcessed(
        calculateBytesProcessed(bytesPerIteration, state.iterations()));
    state.SetItemsProcessed(
        calculateItemsProcessed(batchSize * 2U, state.iterations()));
}

static void poolMixedWorkload(benchmark::State& state) {
    Trading::Core::MemoryPool<PoolObject> memPool(MIXED_POOL_SIZE);
    std::vector<PoolObject*> pointers;
    pointers.reserve(MIXED_POOL_SIZE);

    std::random_device rd;
    std::mt19937 generator(rd());
    std::uniform_int_distribution<std::uint32_t> operationDist(0U, 100U);

    for (auto _ : state) {
        if (operationDist(generator) < ALLOCATION_PROBABILITY) {
            if (pointers.size() >= MIXED_POOL_SIZE) {
                continue;
            }
            auto* obj = memPool.allocate(42);
            benchmark::DoNotOptimize(obj);
            pointers.push_back(obj);
        } else {
            if (pointers.empty()) {
                continue;
            }
            const auto index =
                static_cast<std::size_t>(operationDist(generator)) %
                pointers.size();
            memPool.deallocate(pointers[index]);
            pointers[index] = pointers.back();
            pointers.pop_back();
        }
        benchmark::ClobberMemory();
    }

    state.SetItemsProcessed(static_cast<std::int64_t>(state.iterations()));

    // Cleanup
    for (auto* ptr : pointers) {
        memPool.deallocate(ptr);
    }
}

static void poolContainerGrowth(benchmark::State& state) {
    const auto initialSize = static_cast<std::size_t>(state.range(0));
    Trading::Core::MemoryPool<PoolObject> memPool(initialSize * 2U);
    std::vector<PoolObject*> elements;
    elements.reserve(initialSize);

    constexpr std::size_t REALLOCATION_FREQUENCY = 8U;

    for (auto _ : state) {
        for (std::size_t i = 0U; i < initialSize; ++i) {
            elements.push_back(memPool.allocate(static_cast<int>(i)));
            if (i % REALLOCATION_FREQUENCY == 0U) {
                for (auto* ptr : elements) {
                    memPool.deallocate(ptr);
                }
                elements.clear();
            }
        }

        for (auto* ptr : elements) {
            memPool.deallocate(ptr);
        }
        elements.clear();
        benchmark::ClobberMemory();
    }

    state.SetItemsProcessed(
        calculateItemsProcessed(initialSize, state.iterations()));
}

static void poolExhaustion(benchmark::State& state) {
    const auto poolSize = static_cast<std::size_t>(state.range(0));
    Trading::Core::MemoryPool<PoolObject> memPool(poolSize);
    std::vector<PoolObject*> pointers;
    pointers.reserve(poolSize);

    for (auto _ : state) {
        // Fill pool to capacity
        while (pointers.size() < poolSize) {
            auto* obj = memPool.allocate(42);
            benchmark::DoNotOptimize(obj);
            pointers.push_back(obj);
        }
        benchmark::ClobberMemory();

        // Release half
        const auto halfSize = poolSize / 2U;
        for (std::size_t i = 0U; i < halfSize; ++i) {
            memPool.deallocate(pointers.back());
            pointers.pop_back();
        }
        benchmark::ClobberMemory();

        // Refill
        while (pointers.size() < poolSize) {
            auto* obj = memPool.allocate(42);
            benchmark::DoNotOptimize(obj);
            pointers.push_back(obj);
        }
        benchmark::ClobberMemory();

        // Full cleanup
        while (!pointers.empty()) {
            memPool.deallocate(pointers.back());
            pointers.pop_back();
        }
        benchmark::ClobberMemory();
    }

    state.SetItemsProcessed(
        calculateItemsProcessed(poolSize * 2U, state.iterations()));
}

BENCHMARK(baselineNewDelete)
    ->Arg(SMALL_OBJECT_SIZE)
    ->Arg(MEDIUM_OBJECT_SIZE)
    ->Arg(LARGE_OBJECT_SIZE)
    ->UseRealTime();

BENCHMARK(poolAllocDeallocCycle)
    ->Arg(SMALL_OBJECT_SIZE)
    ->Arg(MEDIUM_OBJECT_SIZE)
    ->Arg(LARGE_OBJECT_SIZE)
    ->UseRealTime();

BENCHMARK(poolSequentialBatch)->Arg(10)->Arg(100)->Arg(1000)->UseRealTime();

BENCHMARK(poolMixedWorkload)->UseRealTime();

BENCHMARK(poolContainerGrowth)->Arg(100)->Arg(1000)->UseRealTime();

BENCHMARK(poolExhaustion)->Arg(100)->Arg(1000)->UseRealTime();
