#include <benchmark/benchmark.h>

#include "module/component.hpp"
#include "module/header_only.hpp"

namespace {

void bmSafeDivide(benchmark::State& state) {
    for (auto _ : state) {
        benchmark::DoNotOptimize(DummyNamespace::safe_divide(144, 12));
    }
}

void bmCounterIncrement(benchmark::State& state) {
    for (auto _ : state) {
        DummyNamespace::Counter counter;
        for (int i = 0; i < state.range(0); ++i) {
            benchmark::DoNotOptimize(counter.increment());
        }
        benchmark::DoNotOptimize(counter.get());
    }
}

}  // namespace

BENCHMARK(bmSafeDivide);
BENCHMARK(bmCounterIncrement)->Arg(1)->Arg(16)->Arg(256);
