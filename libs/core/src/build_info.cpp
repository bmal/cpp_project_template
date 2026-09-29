// Mechanism: a vcpkg dependency found with find_package and linked through the module helper.
// Formats the generated version constants with fmt into the banner every app prints at startup.
#include "core/build_info.hpp"

#include <fmt/format.h>
#include <myproj/version.hpp>

namespace myproj::core {

std::string version_banner() {
    return fmt::format("myproj {} ({}{})", version_string, git_commit, git_dirty ? "-dirty" : "");
}

} // namespace myproj::core
