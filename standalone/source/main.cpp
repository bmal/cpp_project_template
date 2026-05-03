#include <module/component.hpp>
#include <module/header_only.hpp>

int main() {
    Example::Counter counter;
    counter.increment(41);

    const auto result = Example::safe_divide(counter.increment().get(), 6);
    if (!result.has_value())
        return 1;

    return result.value() == 7 ? 0 : 1;
}
