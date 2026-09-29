// Mechanism: one benchmark executable per module, built only by the bench preset.
// parse_line with warm and cold caches, each reporting p50, p90, and p99 over repetitions.
#include <benchmark/benchmark.h>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "bench_support/cache_flush.hpp"
#include "bench_support/percentiles.hpp"
#include "parser/parser.hpp"

namespace {

using myproj::bench_support::add_percentiles;

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): both come from benchmark ranges.
std::string make_line(std::int64_t fields, std::int64_t seed) {
    std::string line;
    for (std::int64_t i = 0; i < fields; ++i) {
        line += "key" + std::to_string(i) + "=value" + std::to_string(seed + i) + " ";
    }
    return line;
}

// The same line on every iteration, so it stays in the L1 cache.
void bm_parse_line(benchmark::State& state) {
    const std::string line = make_line(state.range(0), 0);
    // NOLINTNEXTLINE(clang-analyzer-deadcode.DeadStores): the loop variable only counts runs.
    for (auto _ : state) {
        auto fields = myproj::parser::parse_line(line);
        benchmark::DoNotOptimize(fields);
    }
    state.SetItemsProcessed(state.iterations() * state.range(0));
}

// A batch of distinct lines after a cache flush, as when a burst of input arrives.
// The batch is large enough that the flush, outside the timer, does not dominate the run.
void bm_parse_line_cold(benchmark::State& state) {
    constexpr std::int64_t fields_per_line = 8;
    std::vector<std::string> lines;
    lines.reserve(static_cast<std::size_t>(state.range(0)));
    for (std::int64_t i = 0; i < state.range(0); ++i) {
        lines.push_back(make_line(fields_per_line, i));
    }
    // NOLINTNEXTLINE(clang-analyzer-deadcode.DeadStores): the loop variable only counts runs.
    for (auto _ : state) {
        state.PauseTiming();
        myproj::bench_support::flush_cache();
        state.ResumeTiming();
        for (const std::string& line : lines) {
            auto fields = myproj::parser::parse_line(line);
            benchmark::DoNotOptimize(fields);
        }
    }
    state.SetItemsProcessed(state.iterations() * state.range(0));
}

} // namespace

BENCHMARK(bm_parse_line)->Arg(1)->Arg(16)->Apply(add_percentiles);
BENCHMARK(bm_parse_line_cold)->Arg(16384)->Apply(add_percentiles);
