#pragma once
#include <gtest/gtest.h>

namespace TestUtils {
enum class Sanitizer { ASAN, TSAN, MSAN, UBSAN };

template <Sanitizer S>
constexpr bool isSanitizerEnabled() {
    if constexpr (S == Sanitizer::ASAN) {
#if defined(__SANITIZE_ADDRESS__) || defined(__has_feature)
#if defined(__has_feature) && __has_feature(address_sanitizer)
        return true;
#endif
#endif
    } else if constexpr (S == Sanitizer::TSAN) {
#if defined(__SANITIZE_THREAD__) || defined(__has_feature)
#if defined(__has_feature) && __has_feature(thread_sanitizer)
        return true;
#endif
#endif
    } else if constexpr (S == Sanitizer::MSAN) {
#if defined(__SANITIZE_MEMORY__) || defined(__has_feature)
#if defined(__has_feature) && __has_feature(memory_sanitizer)
        return true;
#endif
#endif
    } else if constexpr (S == Sanitizer::UBSAN) {
#if defined(__SANITIZE_UNDEFINED__) || defined(__has_feature)
#if defined(__has_feature) && __has_feature(undefined_behavior_sanitizer)
        return true;
#endif
#endif
    }
    return false;
}
}  // namespace TestUtils

#define SKIP_UNDER_SANITIZER(sanitizer, reason)                               \
    do {                                                                      \
        if (TestUtils::isSanitizerEnabled<                                    \
                TestUtils::Sanitizer::sanitizer>()) {                         \
            GTEST_SKIP() << "Skipped under " << #sanitizer << ": " << reason; \
        }                                                                     \
    } while (0)

#define SKIP_UNDER_ANY_SANITIZER(reason)                                    \
    do {                                                                    \
        if (TestUtils::isSanitizerEnabled<TestUtils::Sanitizer::ASAN>() ||  \
            TestUtils::isSanitizerEnabled<TestUtils::Sanitizer::TSAN>() ||  \
            TestUtils::isSanitizerEnabled<TestUtils::Sanitizer::MSAN>() ||  \
            TestUtils::isSanitizerEnabled<TestUtils::Sanitizer::UBSAN>()) { \
            GTEST_SKIP() << "Skipped under sanitizer: " << reason;          \
        }                                                                   \
    } while (0)
