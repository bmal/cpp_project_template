// Mechanism: a hand-written fake behind the Clock concept, for code on hot paths that takes a Clock.
// Time moves only when a test calls advance; for tests that already link myproj::core.
#pragma once

#include "core/clock.hpp"

#include <chrono>

namespace myproj::test_support {

class FakeClock {
public:
    [[nodiscard]] std::chrono::nanoseconds now() const noexcept { return now_; }

    void advance(std::chrono::nanoseconds by) noexcept { now_ += by; }

private:
    std::chrono::nanoseconds now_{0};
};

static_assert(core::Clock<FakeClock>);

} // namespace myproj::test_support
