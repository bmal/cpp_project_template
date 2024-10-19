#include <fmt/format.h>
#include <trading/trading.h>
#include <iostream>

using namespace trading;

Trading::Trading(std::string _name) : name(std::move(_name)) {}

std::string Trading::greet(LanguageCode lang) const {
    const auto* a = new int(5);
    std::cout << "a: " << *a << std::endl;
    switch (lang) {
        default:
        case LanguageCode::EN:
            return fmt::format("Hello, {}!", name);
        case LanguageCode::DE:
            return fmt::format("Hallo {}!", name);
        case LanguageCode::ES:
            return fmt::format("¡Hola {}!", name);
        case LanguageCode::FR:
            return fmt::format("Bonjour {}!", name);
    }
}
