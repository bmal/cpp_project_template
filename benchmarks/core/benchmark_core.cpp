#include <benchmark/benchmark.h>
#include <cstdint>

#include "core/counter.hpp"

namespace {

void bm_counter_add(benchmark::State& state) {
    for (auto _ : state) {
        myproj::core::Counter counter;
        for (std::int64_t i = 0; i < state.range(0); ++i) {
            counter.add(static_cast<std::uint64_t>(i));
        }
        benchmark::DoNotOptimize(counter.value());
    }
}

} // namespace

BENCHMARK(bm_counter_add)->Arg(1)->Arg(16)->Arg(256);
