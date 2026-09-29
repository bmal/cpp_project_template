// Mechanism: the cache flush behind bench_support::flush_cache.
// ClobberMemory keeps the compiler from dropping writes that nothing reads.
#include "bench_support/cache_flush.hpp"

#include <benchmark/benchmark.h>
#include <cstddef>
#include <vector>

namespace myproj::bench_support {

void flush_cache() {
    constexpr std::size_t cache_line = 64;
    static std::vector<unsigned char> buffer(flush_bytes);
    for (std::size_t i = 0; i < buffer.size(); i += cache_line) {
        ++buffer[i];
    }
    benchmark::DoNotOptimize(buffer.data());
    benchmark::ClobberMemory();
}

} // namespace myproj::bench_support
