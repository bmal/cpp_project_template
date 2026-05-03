cmake_minimum_required(VERSION 3.20...3.30)

include(../cmake/ProjectIdentity.cmake)

project(${TEMPLATE_TEST_PROJECT} LANGUAGES CXX)

# Options
option(ENABLE_BENCHMARKS "Enable benchmark targets" ON)
option(ENABLE_COVERAGE "Enable test coverage" OFF)
option(TEST_INSTALLED_VERSION "Test the version found by find_package" OFF)

enable_testing()

# Import tools and dependencies
include(../cmake/tools.cmake)
include(../cmake/Coverage.cmake)
include(../cmake/CPM.cmake)

# Add GTest
if(NOT TARGET GTest::gtest)
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
endif()

# Add Google Benchmark if benchmarks are enabled
if(ENABLE_BENCHMARKS)
    if(NOT TARGET benchmark::benchmark)
      CPMAddPackage(
        NAME benchmark
        GITHUB_REPOSITORY google/benchmark
        GIT_TAG v1.8.3
        OPTIONS
            "BENCHMARK_ENABLE_TESTING OFF"
            "BENCHMARK_ENABLE_GTEST_TESTS OFF"
            "BENCHMARK_ENABLE_INSTALL OFF"
      )
    endif()
endif()

# Add main library
if(TARGET ${TEMPLATE_NAMESPACED_TARGET})
    # The tests are being built from the root project.
elseif(TEST_INSTALLED_VERSION)
    find_package(${TEMPLATE_PROJECT_NAME} REQUIRED)
else()
    CPMAddPackage(NAME ${TEMPLATE_PROJECT_NAME} SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/..)
endif()

# Add test directories
add_subdirectory(functional)

if(ENABLE_BENCHMARKS)
    add_subdirectory(benchmark)
endif()
