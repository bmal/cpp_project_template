// Mechanism: a cold-cache measurement helper, called while the benchmark timer is paused.
// It evicts the data caches by writing a buffer larger than any last-level cache.
#pragma once

#include <cstddef>

namespace myproj::bench_support {

// Larger than the last-level cache of current desktop and server CPUs.
inline constexpr std::size_t flush_bytes = std::size_t{64} << 20U;

// Writes one byte per cache line of a flush_bytes buffer. Takes milliseconds, so call it
// between state.PauseTiming() and state.ResumeTiming(), with iterations that take longer.
void flush_cache();

} // namespace myproj::bench_support
