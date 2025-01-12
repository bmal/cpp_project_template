#pragma once

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace utils {
namespace detail {
#ifdef _WIN32
#include <windows.h>
inline void error_write(const char* str) noexcept {
    auto handle = GetStdHandle(STD_ERROR_HANDLE);
    WriteConsoleA(handle, str, strlen(str), nullptr, nullptr);
}
#define DEBUG_TRAP() __debugbreak()
#else
#include <unistd.h>
inline void error_write(const char* str) noexcept {
    write(STDERR_FILENO, str, strlen(str));
}
#define DEBUG_TRAP() __builtin_trap()
#endif
}  // namespace detail

#ifdef NDEBUG
#define ASSERT(condition, message) (void)0
#else
[[noreturn]] inline void assert_fail(const char* condition,
                                     const char* message,
                                     const char* file,
                                     unsigned line) noexcept {
    detail::error_write("ASSERT FAILED: '");
    detail::error_write(condition);
    detail::error_write("' - ");
    detail::error_write(message);
    detail::error_write(" at ");
    detail::error_write(file);
    detail::error_write(":");

    char line_buf[20];
    snprintf(line_buf, sizeof(line_buf), "%u\n", line);
    detail::error_write(line_buf);

#ifdef DEBUG_BREAK_ON_ASSERT
    DEBUG_TRAP();
#endif

    _Exit(EXIT_FAILURE);
}

#define ASSERT(condition, message)                                       \
    do {                                                                 \
        if (!(condition)) [[unlikely]] {                                 \
            utils::assert_fail(#condition, message, __FILE__, __LINE__); \
        }                                                                \
    } while (0)
#endif

[[noreturn]] inline void fatal(const char* message) noexcept {
    detail::error_write("FATAL: ");
    detail::error_write(message);
    detail::error_write("\n");
    _Exit(EXIT_FAILURE);
}

#define FATAL(message) ::utils::fatal(message)

}  // namespace utils
