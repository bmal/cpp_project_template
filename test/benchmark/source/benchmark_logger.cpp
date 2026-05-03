#include <benchmark/benchmark.h>
#include <cstdint>

#include "module/component.hpp"
#include "module/header_only.hpp"

namespace {

void bmSafeDivide(benchmark::State& state) {
    for (auto _ : state) {
        benchmark::DoNotOptimize(Example::safe_divide(144, 12));
    }
}

void bmCounterIncrement(benchmark::State& state) {
    for (auto _ : state) {
        Example::Counter counter;
        for (std::int64_t i = 0; i < state.range(0); ++i) {
            benchmark::DoNotOptimize(counter.increment());
        }
        benchmark::DoNotOptimize(counter.get());
    }
}

}  // namespace

BENCHMARK(bmSafeDivide);
BENCHMARK(bmCounterIncrement)->Arg(1)->Arg(16)->Arg(256);
