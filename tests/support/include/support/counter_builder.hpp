// Mechanism: a test data builder, so a test states only the values that matter to it.
// Starts from a valid default and ends in build(); for tests that already link myproj::core.
#pragma once

#include "core/counter.hpp"

#include <cstdint>

namespace myproj::test_support {

// A core::Counter already holding a total.
class CounterBuilder {
public:
    CounterBuilder& with_total(std::uint64_t total) noexcept {
        total_ = total;
        return *this;
    }

    [[nodiscard]] core::Counter build() const noexcept {
        core::Counter counter;
        counter.add(total_);
        return counter;
    }

private:
    std::uint64_t total_ = 0;
};

} // namespace myproj::test_support
