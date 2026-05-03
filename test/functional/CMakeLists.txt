# Create functional test executable
file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp)
add_executable(TradingTests ${TEST_SOURCES})

target_link_libraries(TradingTests PRIVATE 
    GTest::gtest
    GTest::gmock
    Trading::Trading
)

# Configure compiler flags
target_compile_features(TradingTests PRIVATE cxx_std_23)
set_target_properties(
    TradingTests
    PROPERTIES CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF
)

if(NOT TEST_INSTALLED_VERSION)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        target_compile_options(TradingTests PRIVATE -Wall -Wpedantic -Wextra -Werror)
    elseif(MSVC)
        target_compile_options(TradingTests PRIVATE /W4 /WX)
    endif()
endif()

include(GoogleTest)
gtest_discover_tests(TradingTests)

# Handle sanitizer suppressions
if(USE_SANITIZER MATCHES "([Mm]emory.*)")
    target_compile_options(TradingTests PRIVATE -fno-sanitize=memory)
    target_link_options(TradingTests PRIVATE -fno-sanitize=memory)
endif()

# Handle code coverage
if(ENABLE_COVERAGE)
    enable_coverage(TradingTests)
endif()

if(USE_SANITIZER)
    if(USE_SANITIZER MATCHES "([Aa]ddress)")
        target_compile_definitions(TradingTests PRIVATE __SANITIZE_ADDRESS__=1)
    endif()
    if(USE_SANITIZER MATCHES "([Tt]hread)")
        target_compile_definitions(TradingTests PRIVATE __SANITIZE_THREAD__=1)
    endif()
    if(USE_SANITIZER MATCHES "([Mm]emory)")
        target_compile_definitions(TradingTests PRIVATE __SANITIZE_MEMORY__=1)
    endif()
    if(USE_SANITIZER MATCHES "([Uu]ndefined)")
        target_compile_definitions(TradingTests PRIVATE __SANITIZE_UNDEFINED__=1)
    endif()
endif()
