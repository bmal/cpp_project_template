#include "core/component.hpp"

namespace myproj {

Counter& Counter::increment(int by) {
    value += by;
    return *this;
}

Counter& Counter::decrement(int by) {
    value -= by;
    return *this;
}

std::expected<int, std::string> Counter::checked_decrement(int by) {
    if (value - by < 0)
        return std::unexpected("underflow: result would be negative");
    value -= by;
    return value;
}

int Counter::get() const {
    return value;
}

void Counter::reset() {
    value = 0;
}

}  // namespace myproj
