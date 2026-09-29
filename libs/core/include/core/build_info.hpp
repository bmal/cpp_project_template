// Mechanism: the generated version header reached through a function, so callers need no macros.
// The version and git commit are captured when the project is configured.
#pragma once

#include <string>

namespace myproj::core {

// Returns "myproj <version> (<short commit>)", with "-dirty" after the commit for local changes.
[[nodiscard]] std::string version_banner();

} // namespace myproj::core
