cmake_minimum_required(VERSION 3.14...3.22)

project(TradingTests LANGUAGES CXX)

# ---- Options ----

option(ENABLE_COVERAGE "Enable test coverage" OFF)
option(TEST_INSTALLED_VERSION "Test the version found by find_package" OFF)

# --- Import tools ----

include(../cmake/tools.cmake)
include(../cmake/Coverage.cmake)

# ---- Dependencies ----

include(../cmake/CPM.cmake)

CPMAddPackage("gh:doctest/doctest@2.4.9")
CPMAddPackage("gh:TheLartians/Format.cmake@1.7.3")

if(TEST_INSTALLED_VERSION)
  find_package(Trading REQUIRED)
else()
  CPMAddPackage(NAME Trading SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/..)
endif()

# ---- Create binary ----

file(GLOB sources CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp)
add_executable(${PROJECT_NAME} ${sources})
target_link_libraries(${PROJECT_NAME} PRIVATE doctest::doctest Trading::Trading)
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 23)

# enable compiler warnings
if(NOT TEST_INSTALLED_VERSION)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wpedantic -Wextra -Werror)
  elseif(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /W4 /WX)
    target_compile_definitions(${PROJECT_NAME} PRIVATE DOCTEST_CONFIG_USE_STD_HEADERS)
  endif()
endif()

# ---- Add TradingTests ----

enable_testing()

# Note: doctest and similar testing frameworks can automatically configure CMake tests. For other
# testing frameworks add the tests target instead: add_test(NAME ${PROJECT_NAME} COMMAND
# ${PROJECT_NAME})

include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)
doctest_discover_tests(${PROJECT_NAME})

# ---- Supression for sanitizers ----
if(USE_SANITIZER MATCHES "([Mm]emory.*)")
    target_compile_options(${PROJECT_NAME} PRIVATE -fno-sanitize=memory)
    target_link_options(${PROJECT_NAME} PRIVATE -fno-sanitize=memory)
endif()

# ---- code coverage ----

if(ENABLE_TEST_COVERAGE)
    enable_coverage(${PROJECT_NAME})
endif()
