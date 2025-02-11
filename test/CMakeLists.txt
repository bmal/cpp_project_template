cmake_minimum_required(VERSION 3.14...3.22)
project(TradingTests LANGUAGES CXX)

# Options
option(ENABLE_BENCHMARKS "Enable benchmark targets" ON)
option(ENABLE_COVERAGE "Enable test coverage" OFF)
option(TEST_INSTALLED_VERSION "Test the version found by find_package" OFF)

# Import tools and dependencies
include(../cmake/tools.cmake)
include(../cmake/Coverage.cmake)
include(../cmake/CPM.cmake)

# Add GTest
CPMAddPackage(
    NAME GTest
    GITHUB_REPOSITORY google/googletest
    GIT_TAG v1.14.0
    VERSION 1.14.0
    OPTIONS 
        "INSTALL_GTEST OFF"
        "gtest_force_shared_crt ON"
        "BUILD_GMOCK ON"
)

# Add Google Benchmark if benchmarks are enabled
if(ENABLE_BENCHMARKS)
    CPMAddPackage(
        NAME benchmark
        GITHUB_REPOSITORY google/benchmark
        GIT_TAG v1.8.3
        OPTIONS
            "BENCHMARK_ENABLE_TESTING OFF"
            "BENCHMARK_ENABLE_GTEST_TESTS OFF"
    )
endif()

# Add main library
if(TEST_INSTALLED_VERSION)
    find_package(Trading REQUIRED)
else()
    CPMAddPackage(NAME Trading SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/..)
endif()

# Add test directories
add_subdirectory(functional)

if(ENABLE_BENCHMARKS)
    add_subdirectory(benchmark)
endif()
