// Mechanism: a scope that fails the running test if anything in it allocates on the heap.
// Proves a hot path allocates nothing; allocation_guard.cpp explains how allocations are counted.
#pragma once

#include <cstddef>
#include <source_location>

namespace myproj::test_support {

// Every global operator new since the program started, on every thread.
[[nodiscard]] std::size_t allocations_so_far() noexcept;

// Records a test failure at its own declaration when its scope allocated at least once.
class AllocationGuard {
public:
    explicit AllocationGuard(std::source_location where = std::source_location::current()) noexcept;
    ~AllocationGuard();

    AllocationGuard(const AllocationGuard&) = delete;
    AllocationGuard& operator=(const AllocationGuard&) = delete;
    AllocationGuard(AllocationGuard&&) = delete;
    AllocationGuard& operator=(AllocationGuard&&) = delete;

    // Allocations since construction.
    [[nodiscard]] std::size_t allocations() const noexcept;

private:
    std::source_location where_;
    std::size_t start_;
};

} // namespace myproj::test_support
