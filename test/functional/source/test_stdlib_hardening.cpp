// Proves the dev preset turns on standard library hardening on libstdc++ and libc++.
// An out-of-bounds operator[] on std::vector must abort instead of reading past the end.
#include <gtest/gtest.h>

#include <cstddef>
#include <vector>

#if defined(_LIBCPP_VERSION)
#define MYPROJ_STDLIB_HARDENED (_LIBCPP_HARDENING_MODE != _LIBCPP_HARDENING_MODE_NONE)
#elif defined(_GLIBCXX_ASSERTIONS)
#define MYPROJ_STDLIB_HARDENED 1
#else
#define MYPROJ_STDLIB_HARDENED 0
#endif

namespace {

TEST(StdlibHardening, OutOfBoundsVectorIndexAborts) {
#if MYPROJ_STDLIB_HARDENED
    const std::vector<int> values(3);
    const std::size_t past_end = values.size();
    EXPECT_DEATH(static_cast<void>(values[past_end]), "");
#else
    GTEST_SKIP() << "standard library hardening is off in this preset";
#endif
}

}  // namespace
