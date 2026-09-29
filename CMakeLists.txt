cmake_minimum_required(VERSION 3.28)

# Benchmark dependencies are a vcpkg manifest feature, installed only for builds that need them.
if(PROJECT_BUILD_BENCHMARKS)
  list(APPEND VCPKG_MANIFEST_FEATURES benchmarks)
endif()

project(
  MyProj
  VERSION 1.0.0
  LANGUAGES CXX
)

if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
  message(
    FATAL_ERROR
      "In-source builds not allowed. Please make a new directory (called a build directory) and run CMake from there."
  )
endif()

option(PROJECT_BUILD_APPS "Build apps/" ${PROJECT_IS_TOP_LEVEL})
option(PROJECT_BUILD_TESTS "Build tests/" ${PROJECT_IS_TOP_LEVEL})
option(PROJECT_BUILD_BENCHMARKS "Build benchmarks/" OFF)
option(PROJECT_BUILD_FUZZ "Build the libFuzzer harnesses under tests/fuzz/; needs Clang" OFF)
set(PROJECT_FUZZ_SECONDS 30 CACHE STRING "Seconds the run_fuzz target spends on each harness")
option(PROJECT_INSTALL "Generate install and package rules" ${PROJECT_IS_TOP_LEVEL})
option(ENABLE_COVERAGE "Enable test coverage" OFF)

set(CMAKE_CXX_EXTENSIONS OFF)
# No source uses C++20 modules. Scanning would put build-time .modmap files into
# compile_commands.json, which clang-tidy and clangd cannot read before the first build.
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
if(PROJECT_IS_TOP_LEVEL)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")
endif()

include(cmake/CompilerPolicy.cmake)
include(cmake/Sanitizers.cmake)
include(cmake/ProjectModules.cmake)

# ---- Dependencies come from vcpkg.json through the toolchain in cmake/toolchain.cmake ----

find_package(fmt CONFIG REQUIRED)

if(PROJECT_BUILD_TESTS)
  enable_testing()
  find_package(GTest CONFIG REQUIRED)
  include(GoogleTest)
  include(cmake/Coverage.cmake)
  add_subdirectory(tests/support)
endif()

add_subdirectory(libs)
project_check_unit_test_dirs()

if(PROJECT_BUILD_APPS)
  add_subdirectory(apps)
endif()

if(PROJECT_BUILD_TESTS)
  add_subdirectory(tests/integration)
  if(PROJECT_BUILD_APPS)
    add_subdirectory(tests/functional)
  endif()
endif()

if(PROJECT_BUILD_BENCHMARKS)
  find_package(benchmark CONFIG REQUIRED)
  add_subdirectory(benchmarks)
endif()

if(PROJECT_BUILD_FUZZ)
  enable_testing()
  add_subdirectory(tests/fuzz)
  project_add_fuzz_run_target()
endif()

if(PROJECT_IS_TOP_LEVEL)
  include(cmake/Lint.cmake)
  project_add_lint_target()
endif()

if(PROJECT_INSTALL)
  project_install_package()
  if(PROJECT_IS_TOP_LEVEL)
    include(CPack)
  endif()
endif()
