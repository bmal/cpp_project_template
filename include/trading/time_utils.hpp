#pragma once

#include <chrono>
#include <cstdint>
#include <ctime>
#include <string>

namespace Trading::Core {
constexpr int64_t NANOS_TO_MICROS = 1000;
constexpr int64_t MICROS_TO_MILLIS = 1000;
constexpr int64_t MILLIS_TO_SECS = 1000;
constexpr int64_t NANOS_TO_MILLIS = NANOS_TO_MICROS * MICROS_TO_MILLIS;
constexpr int64_t NANOS_TO_SECS = NANOS_TO_MILLIS * MILLIS_TO_SECS;

[[nodiscard]] inline int64_t getCurrentNanos() noexcept {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

inline std::string getCurrentTimeStr() {
    const auto time =
        std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::string result(ctime(&time));
    if (!result.empty()) {
        result.pop_back();
    }
    return result;
}
}  // namespace Trading::Core
