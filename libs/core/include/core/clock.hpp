// Mechanism: a concept as a compile-time seam; code takes any Clock and tests inject a fake.
// SystemClock is the production model of the concept, and Stopwatch is code written against it.
#pragma once

#include <chrono>
#include <concepts>

namespace myproj::core {

// A source of monotonic time, measured from an arbitrary but fixed epoch.
template <typename T>
concept Clock = requires(const T& clock) {
    { clock.now() } -> std::same_as<std::chrono::nanoseconds>;
};

// The steady clock of the standard library, the only clock that production code uses.
class SystemClock {
public:
    [[nodiscard]] static std::chrono::nanoseconds now() noexcept;
};

static_assert(Clock<SystemClock>);

// Measures time elapsed since construction or the last restart on a clock it does not own.
template <Clock C>
class Stopwatch {
public:
    explicit Stopwatch(const C& clock) noexcept : clock_(&clock), start_(clock.now()) {}
    // A temporary clock would be gone before the first call to elapsed.
    explicit Stopwatch(const C&& clock) = delete;

    [[nodiscard]] std::chrono::nanoseconds elapsed() const noexcept {
        return clock_->now() - start_;
    }

    void restart() noexcept { start_ = clock_->now(); }

private:
    const C* clock_;
    std::chrono::nanoseconds start_;
};

} // namespace myproj::core
