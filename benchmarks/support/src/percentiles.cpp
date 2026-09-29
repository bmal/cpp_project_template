// Mechanism: nearest-rank percentiles as Google Benchmark statistics functions.
// Nearest rank returns a measured value, never an interpolation between two runs.
#include "bench_support/percentiles.hpp"

#include <algorithm>
#include <cstddef>
#include <vector>

namespace myproj::bench_support {
namespace {

template <std::size_t Percent>
double percentile(const std::vector<double>& samples) {
    if (samples.empty()) {
        return 0.0;
    }
    std::vector<double> sorted = samples;
    std::ranges::sort(sorted);
    const std::size_t rank = ((Percent * sorted.size()) + 99) / 100;
    return sorted[std::max<std::size_t>(rank, 1) - 1];
}

} // namespace

void add_percentiles(benchmark::Benchmark* bench) {
    bench->ComputeStatistics("p50", percentile<50>);
    bench->ComputeStatistics("p90", percentile<90>);
    bench->ComputeStatistics("p99", percentile<99>);
}

} // namespace myproj::bench_support
