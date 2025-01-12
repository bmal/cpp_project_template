#pragma once

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace sysUtils {
namespace detail {
#ifdef _WIN32
#include <windows.h>
inline void writeError(const char* str) noexcept {
    auto handle = GetStdHandle(STD_ERROR_HANDLE);
    WriteConsoleA(handle, str, strlen(str), nullptr, nullptr);
}
#define DEBUG_TRAP() __debugbreak()
#else
#include <unistd.h>
inline void writeError(const char* str) noexcept {
    write(STDERR_FILENO, str, strlen(str));
}
#define DEBUG_TRAP() __builtin_trap()
#endif
}  // namespace detail

#ifdef NDEBUG
#define ASSERT(condition, message) (void)0
#else
[[noreturn]] inline void assertFail(const char* condition,
                                    const char* message,
                                    const char* file,
                                    unsigned line) noexcept {
    detail::writeError("ASSERT FAILED: '");
    detail::writeError(condition);
    detail::writeError("' - ");
    detail::writeError(message);
    detail::writeError(" at ");
    detail::writeError(file);
    detail::writeError(":");

    char lineBuf[20];
    snprintf(lineBuf, sizeof(lineBuf), "%u\n", line);
    detail::writeError(lineBuf);

#ifdef DEBUG_BREAK_ON_ASSERT
    DEBUG_TRAP();
#endif

    _Exit(EXIT_FAILURE);
}

#define ASSERT(condition, message)                                         \
    /* NOLINTBEGIN(cppcoreguidelines-avoid-do-while) */                    \
    do {                                                                   \
        if (!(condition)) [[unlikely]] {                                   \
            sysUtils::assertFail(#condition, message, __FILE__, __LINE__); \
        }                                                                  \
    } while (0)  // NOLINTEND(cppcoreguidelines-avoid-do-while)
#endif

[[noreturn]] inline void fatal(const char* message) noexcept {
    detail::writeError("FATAL: ");
    detail::writeError(message);
    detail::writeError("\n");
    _Exit(EXIT_FAILURE);
}

#define FATAL(message) ::sysUtils::fatal(message)

}  // namespace sysUtils
