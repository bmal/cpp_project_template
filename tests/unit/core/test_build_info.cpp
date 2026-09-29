// Unit tests of core::version_banner: the banner reflects the generated version header.
// Output-based: the test compares strings and knows nothing about how the banner is built.
#include <gtest/gtest.h>

#include "core/build_info.hpp"

#include <myproj/version.hpp>

#include <string>

namespace myproj::core {

TEST(VersionBanner, NamesTheVersionAndCommit) {
    const std::string banner = version_banner();

    EXPECT_TRUE(banner.starts_with("myproj " + std::string(version_string) + " ("));
    EXPECT_NE(banner.find(git_commit), std::string::npos);
}

} // namespace myproj::core
