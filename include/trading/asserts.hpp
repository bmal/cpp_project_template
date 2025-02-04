#pragma once

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace Common {
namespace Detail {
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
    [[maybe_unused]] auto _ = write(STDERR_FILENO, str, strlen(str));
}
#define DEBUG_TRAP() __builtin_trap()
#endif
}  // namespace Detail

#ifdef NDEBUG
#define ASSERT(condition, message)
#else
[[noreturn]] inline void assertFail(const char* condition,
                                    const char* message,
                                    const char* file,
                                    unsigned line) noexcept {
    Detail::writeError("ASSERT FAILED: '");
    Detail::writeError(condition);
    Detail::writeError("' - ");
    Detail::writeError(message);
    Detail::writeError(" at ");
    Detail::writeError(file);
    Detail::writeError(":");

    char lineBuf[20];
    snprintf(lineBuf, sizeof(lineBuf), "%u\n", line);
    Detail::writeError(lineBuf);

#ifdef DEBUG_BREAK_ON_ASSERT
    DEBUG_TRAP();
#endif

    _Exit(EXIT_FAILURE);
}

#define ASSERT(condition, message)                                       \
    /* NOLINTBEGIN(cppcoreguidelines-avoid-do-while) */                  \
    do {                                                                 \
        if (!(condition)) [[unlikely]] {                                 \
            Common::assertFail(#condition, message, __FILE__, __LINE__); \
        }                                                                \
    } while (0)  // NOLINTEND(cppcoreguidelines-avoid-do-while)
#endif

[[noreturn]] inline void fatal(const char* message) noexcept {
    Detail::writeError("FATAL: ");
    Detail::writeError(message);
    Detail::writeError("\n");
    _Exit(EXIT_FAILURE);
}

#define FATAL(message) ::Common::fatal(message)

}  // namespace Common
