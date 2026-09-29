// Mechanism: the definitions of a stateful type, compiled once into myproj_core.
// Only subtract can fail, and it reports failure without touching the state.
#include "core/counter.hpp"

#include <cstdint>
#include <expected>
#include <string_view>

namespace myproj::core {

std::string_view describe(CounterError error) noexcept {
    switch (error) {
    case CounterError::Underflow:
        return "underflow: the total would be negative";
    }
    return "unknown counter error";
}

void Counter::add(std::uint64_t amount) noexcept {
    value_ += amount;
}

std::expected<std::uint64_t, CounterError> Counter::subtract(std::uint64_t amount) noexcept {
    if (amount > value_) {
        return std::unexpected(CounterError::Underflow);
    }
    value_ -= amount;
    return value_;
}

std::uint64_t Counter::value() const noexcept {
    return value_;
}

void Counter::reset() noexcept {
    value_ = 0;
}

} // namespace myproj::core
