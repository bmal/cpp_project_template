// Proves the dev preset turns on standard library hardening on libstdc++ and libc++.
// An out-of-bounds operator[] on std::vector must abort instead of reading past the end.
#include <gtest/gtest.h>

#include "support/sanitizers.hpp"

#include <cstddef>
#include <vector>

#ifdef _LIBCPP_VERSION
#define MYPROJ_STDLIB_HARDENED (_LIBCPP_HARDENING_MODE != _LIBCPP_HARDENING_MODE_NONE)
#elifdef _GLIBCXX_ASSERTIONS
#define MYPROJ_STDLIB_HARDENED 1
#else
#define MYPROJ_STDLIB_HARDENED 0
#endif

namespace {

// A function, not a constant, so no preset sees the code after the skip as unreachable.
bool stdlib_is_hardened() { return MYPROJ_STDLIB_HARDENED != 0; }

TEST(StdlibHardening, OutOfBoundsVectorIndexAborts) {
    MYPROJ_SKIP_UNDER_SANITIZER(TSAN, "a death test forks a process that already runs threads");
    // Every preset compiles the body, so lint reads the same code in each of them.
    if (!stdlib_is_hardened()) {
        GTEST_SKIP() << "standard library hardening is off in this preset";
    }
    const std::vector<int> values(3);
    const std::size_t past_end = values.size();
    EXPECT_DEATH(static_cast<void>(values[past_end]), "");
}

} // namespace
