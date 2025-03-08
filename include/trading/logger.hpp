#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>
#include "trading/asserts.hpp"
#include "trading/spsc_queue.hpp"
#include "trading/thread_utils.hpp"
#include "trading/time_utils.hpp"

namespace Trading::Core {

constexpr size_t LOG_QUEUE_SIZE = 8 * 1024 * 1024;

enum class LogType : int8_t {
    CHAR,
    INTEGER,
    LONG_INTEGER,
    LONG_LONG_INTEGER,
    UNSIGNED_INTEGER,
    UNSIGNED_LONG_INTEGER,
    UNSIGNED_LONG_LONG_INTEGER,
    FLOAT,
    DOUBLE
};

struct LogElement {
    LogType type = LogType::CHAR;
    union {
        char c;
        int i;
        long l;
        long long ll;
        unsigned u;
        unsigned long ul;
        unsigned long long ull;
        float f;
        double d;
    } u;
};

class Logger {
   public:
    explicit Logger(const std::string& filename)
        : FILENAME(filename), _queue(LOG_QUEUE_SIZE) {
        _file.open(filename);
        if (!_file.is_open()) {
            FATAL("Could not open log file:" + filename);
        }

        _loggerThread = createPinnedThread(-1, "Common/Logger " + filename,
                                           [this]() { processMessages(); });
    }

    ~Logger() {
        _isRunning.store(false, std::memory_order_release);

        if (_loggerThread.joinable()) {
            _loggerThread.join();
        }

        _file.close();
    }

    void enqueueMessage(const LogElement& message) noexcept {
        while (!_queue.tryPush(message)) {
            pauseCpu();
        }
    }

    void enqueueMessage(const char value) noexcept {
        enqueueMessage(LogElement{.type = LogType::CHAR, .u = {.c = value}});
    }

    void enqueueMessage(const int value) noexcept {
        enqueueMessage(LogElement{.type = LogType::INTEGER, .u = {.i = value}});
    }

    void enqueueMessage(const long value) noexcept {
        enqueueMessage(
            LogElement{.type = LogType::LONG_INTEGER, .u = {.l = value}});
    }

    void enqueueMessage(const long long value) noexcept {
        enqueueMessage(
            LogElement{.type = LogType::LONG_LONG_INTEGER, .u = {.ll = value}});
    }

    void enqueueMessage(const unsigned value) noexcept {
        enqueueMessage(
            LogElement{.type = LogType::UNSIGNED_INTEGER, .u = {.u = value}});
    }

    void enqueueMessage(const unsigned long value) noexcept {
        enqueueMessage(LogElement{.type = LogType::UNSIGNED_LONG_INTEGER,
                                  .u = {.ul = value}});
    }

    void enqueueMessage(const unsigned long long value) noexcept {
        enqueueMessage(LogElement{.type = LogType::UNSIGNED_LONG_LONG_INTEGER,
                                  .u = {.ull = value}});
    }

    void enqueueMessage(const float value) noexcept {
        enqueueMessage(LogElement{.type = LogType::FLOAT, .u = {.f = value}});
    }

    void enqueueMessage(const double value) noexcept {
        enqueueMessage(LogElement{.type = LogType::DOUBLE, .u = {.d = value}});
    }

    void enqueueMessage(const char* value) noexcept {
        while (*value) {
            enqueueMessage(*value);
            ++value;
        }
    }

    void enqueueMessage(const std::string& value) noexcept {
        enqueueMessage(value.c_str());
    }

    template <typename T, typename... A>
    void log(const char* s, const T& value, A... args) noexcept {
        while (*s) {
            if (*s == '%') {
                [[unlikely]] if (*(s + 1) == '%') {
                    ++s;
                } else {
                    enqueueMessage(value);
                    log(s + 1, args...);
                    return;
                }
            }
            enqueueMessage(*s++);
        }
        FATAL("extra arguments provided to log()");
    }

    void log(const char* s) noexcept {
        while (*s) {
            if (*s == '%') {
                [[unlikely]] if (*(s + 1) == '%') {
                    ++s;
                } else {
                    FATAL("missing arguments to log()");
                }
            }
            enqueueMessage(*s++);
        }
    }

    Logger() = delete;
    Logger(const Logger&) = delete;
    Logger(const Logger&&) = delete;
    Logger& operator=(const Logger&) = delete;
    Logger& operator=(const Logger&&) = delete;

   private:
    static void pauseCpu() noexcept {
#if defined(__x86_64__) || defined(__i386__) || defined(_M_IX86) || \
    defined(_M_X64)
#if defined(_MSC_VER)
        _mm_pause();
#elif defined(__GNUC__) || defined(__clang__)
        __builtin_ia32_pause();
#else
#error "Unsupported compiler for x86 pause instruction"
#endif
#elif defined(__aarch64__) || defined(_M_ARM64)
#if defined(_MSC_VER)
        __yield();
#elif defined(__GNUC__) || defined(__clang__)
        asm volatile("yield" ::: "memory");
#else
#error "Unsupported compiler for ARM64 yield instruction"
#endif
#else
#error "Logger requires x86_64 or ARM64 for predictable pause behavior"
#endif
    }

    void processMessages() noexcept {
        LogElement element;
        while (true) {
            while (_queue.tryPop(element)) {
                processMessage(element);
            }

            if (!_isRunning.load(std::memory_order_acquire)) {
                break;
            }

            _file.flush();
            pauseCpu();
        }

        _file.flush();
    }

    void processMessage(const LogElement& message) noexcept {
        switch (message.type) {
            case LogType::CHAR:
                _file << message.u.c;
                break;
            case LogType::INTEGER:
                _file << message.u.i;
                break;
            case LogType::LONG_INTEGER:
                _file << message.u.l;
                break;
            case LogType::LONG_LONG_INTEGER:
                _file << message.u.ll;
                break;
            case LogType::UNSIGNED_INTEGER:
                _file << message.u.u;
                break;
            case LogType::UNSIGNED_LONG_INTEGER:
                _file << message.u.ul;
                break;
            case LogType::UNSIGNED_LONG_LONG_INTEGER:
                _file << message.u.ull;
                break;
            case LogType::FLOAT:
                _file << std::setprecision(8) << message.u.f;
                break;
            case LogType::DOUBLE:
                _file << std::setprecision(8) << message.u.d;
                break;
        }
    }

    const std::string FILENAME;
    std::ofstream _file;
    SPSCQueue<LogElement> _queue;
    std::atomic<bool> _isRunning{true};
    std::thread _loggerThread;
};

}  // namespace Trading::Core
