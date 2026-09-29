// Mechanism: tail latency reported next to the mean, as Google Benchmark aggregate statistics.
// They are taken over repetitions, so run with --benchmark_repetitions; run_benchmarks does.
#pragma once

#include <benchmark/benchmark.h>

namespace myproj::bench_support {

// Adds p50, p90, and p99 to a benchmark: BENCHMARK(bm_x)->Apply(add_percentiles).
void add_percentiles(benchmark::Benchmark* bench);

} // namespace myproj::bench_support
