// Mechanism: a small stateful type whose failure is a value, returned in std::expected.
// Private state carries a trailing underscore; errors are an enum class with CamelCase values.
#pragma once

#include <cstdint>
#include <expected>
#include <string_view>

namespace myproj::core {

enum class CounterError : std::uint8_t { Underflow };

[[nodiscard]] std::string_view describe(CounterError error) noexcept;

// A running total; unsigned amounts keep it from ever going below zero.
class Counter {
public:
    void add(std::uint64_t amount = 1) noexcept;

    // Returns the new total, or CounterError::Underflow and leaves the total unchanged.
    [[nodiscard]] std::expected<std::uint64_t, CounterError>
    subtract(std::uint64_t amount) noexcept;

    [[nodiscard]] std::uint64_t value() const noexcept;

    void reset() noexcept;

private:
    std::uint64_t value_ = 0;
};

} // namespace myproj::core
