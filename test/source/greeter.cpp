#include <doctest/doctest.h>
#include <trading/trading.h>
#include <trading/version.h>

#include <string>

TEST_CASE("Trading") {
  using namespace trading;

  Trading trading("Tests");

  CHECK(trading.greet(LanguageCode::EN) == "Hello, Tests!");
  CHECK(trading.greet(LanguageCode::DE) == "Hallo Tests!");
  CHECK(trading.greet(LanguageCode::ES) == "¡Hola Tests!");
  CHECK(trading.greet(LanguageCode::FR) == "Bonjour Tests!");
}

TEST_CASE("Trading version") {
  static_assert(std::string_view(GREETER_VERSION) == std::string_view("1.0"));
  CHECK(std::string(GREETER_VERSION) == std::string("1.0"));
}
