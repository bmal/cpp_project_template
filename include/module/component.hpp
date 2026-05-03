#pragma once

#include <expected>
#include <string>

namespace DummyNamespace {

// Declaration lives in the header, definitions in component.cpp.
// This is the pattern we're validating the project template handles correctly.
struct Counter {
    int value = 0;

    Counter& increment(int by = 1);
    Counter& decrement(int by = 1);

    // std::expected as a C++23 feature: returns the new value on success,
    // or an error string if the operation would underflow.
    [[nodiscard]] std::expected<int, std::string> checked_decrement(int by = 1);

    [[nodiscard]] int get() const;
    void reset();
};

}  // namespace DummyNamespace
