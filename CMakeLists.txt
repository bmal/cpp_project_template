cmake_minimum_required(VERSION 3.28)

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
option(PROJECT_INSTALL "Generate install and package rules" ${PROJECT_IS_TOP_LEVEL})
option(ENABLE_COVERAGE "Enable test coverage" OFF)
set(PROJECT_SANITIZE "" CACHE STRING "Sanitizers for -fsanitize=, for example address,undefined")

if(PROJECT_SANITIZE)
  add_compile_options(-fsanitize=${PROJECT_SANITIZE} -fno-sanitize-recover=all -fno-omit-frame-pointer)
  add_link_options(-fsanitize=${PROJECT_SANITIZE})
endif()

set(CMAKE_CXX_EXTENSIONS OFF)
if(PROJECT_IS_TOP_LEVEL)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin")
endif()

include(cmake/CompilerPolicy.cmake)
include(cmake/ProjectModules.cmake)

# ---- Dependencies come from vcpkg.json through the toolchain in cmake/toolchain.cmake ----

find_package(fmt CONFIG REQUIRED)

if(PROJECT_BUILD_TESTS)
  enable_testing()
  find_package(GTest CONFIG REQUIRED)
  include(GoogleTest)
  include(cmake/Coverage.cmake)
endif()

add_subdirectory(libs)
project_check_unit_test_dirs()

if(PROJECT_BUILD_APPS)
  add_subdirectory(apps)
endif()

if(PROJECT_BUILD_BENCHMARKS)
  find_package(benchmark CONFIG REQUIRED)
  add_subdirectory(benchmarks/core)
endif()

if(PROJECT_INSTALL)
  project_install_package()
  if(PROJECT_IS_TOP_LEVEL)
    include(CPack)
  endif()
endif()
