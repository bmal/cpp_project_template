#include <fmt/format.h>
#include <trading/trading.h>
#include <future>
#include <iostream>

using namespace trading;

Trading::Trading(std::string _name) : name(std::move(_name)) {}

std::string Trading::greet(LanguageCode lang) const {
    int a = 1;
    auto t1 = std::async(std::launch::async, [&]() { a = 2; });
    auto t2 = std::async(std::launch::async, [&]() { a = 3; });

    std::cout << "DUPA" << a << std::endl;
    t1.get();
    t2.get();

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
