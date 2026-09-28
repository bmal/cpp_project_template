#pragma once

#include <expected>
#include <string>

namespace myproj {

[[nodiscard]] inline std::expected<int, std::string> safe_divide(int a, int b) {
    if (b == 0)
        return std::unexpected("division by zero");
    return a / b;
}

}  // namespace myproj
