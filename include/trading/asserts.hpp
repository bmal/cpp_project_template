#pragma once

#include <cstdlib>
#include <string>

namespace Common {
namespace Detail {
#include <unistd.h>

inline void writeError(const std::string& str) noexcept {
    [[maybe_unused]] auto _ = write(STDERR_FILENO, str.c_str(), str.length());
}
#define DEBUG_TRAP() __builtin_trap()
}  // namespace Detail

#ifdef NDEBUG
#define ASSERT(condition, message)
#else
[[noreturn]] inline void assertFail(const std::string& condition,
                                    const std::string& message,
                                    const std::string& file,
                                    unsigned line) noexcept {
    Detail::writeError("ASSERT FAILED: '" + condition + "' - " + message +
                       " at " + file + ":" + std::to_string(line) + "\n");

#ifdef DEBUG_BREAK_ON_ASSERT
    DEBUG_TRAP();
#endif

    _Exit(EXIT_FAILURE);
}

#define ASSERT(condition, message)                                       \
    do {                                                                 \
        if (!(condition)) [[unlikely]] {                                 \
            Common::assertFail(#condition, message, __FILE__, __LINE__); \
        }                                                                \
    } while (0)
#endif

[[noreturn]] inline void fatal(const std::string& message) noexcept {
    Detail::writeError("FATAL: " + message + "\n");
    _Exit(EXIT_FAILURE);
}

#define FATAL(message) ::Common::fatal(message)

}  // namespace Common
