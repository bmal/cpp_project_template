// Mechanism: a definition kept out of the header, so myproj_core compiles to a library.
// SystemClock reads std::chrono::steady_clock, which never jumps backwards.
#include "core/clock.hpp"

namespace myproj::core {

std::chrono::nanoseconds SystemClock::now() const noexcept {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch());
}

} // namespace myproj::core
