cmake_minimum_required(VERSION 3.28)

include(cmake/ProjectIdentity.cmake)

# ---- Project ----

project(
  ${TEMPLATE_PROJECT_NAME}
  VERSION 1.0
  LANGUAGES CXX
)

option(PROJECT_BUILD_STANDALONE "Build the standalone executable" OFF)
option(PROJECT_BUILD_TESTS "Build functional tests" OFF)
option(PROJECT_BUILD_BENCHMARKS "Build benchmark targets with the tests" OFF)
set(PROJECT_SANITIZE "" CACHE STRING "Sanitizers for -fsanitize=, for example address,undefined")

# ---- Include guards ----

if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
  message(
    FATAL_ERROR
      "In-source builds not allowed. Please make a new directory (called a build directory) and run CMake from there."
  )
endif()

if(PROJECT_SANITIZE)
  add_compile_options(-fsanitize=${PROJECT_SANITIZE} -fno-sanitize-recover=all -fno-omit-frame-pointer)
  add_link_options(-fsanitize=${PROJECT_SANITIZE})
endif()

include(cmake/CompilerPolicy.cmake)

# ---- Dependencies come from vcpkg.json through the toolchain in cmake/toolchain.cmake ----

find_package(fmt CONFIG REQUIRED)

# ---- Add source files ----

# Note: globbing sources is considered bad practice as CMake's generators may not detect new files
# automatically. Keep that in mind when changing files, or explicitly mention them here.
file(
  GLOB_RECURSE
  headers
  CONFIGURE_DEPENDS
  "${PROJECT_SOURCE_DIR}/include/*.h"
  "${PROJECT_SOURCE_DIR}/include/*.hpp"
)
file(GLOB_RECURSE sources CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/source/*.cpp")

# ---- Create library ----

# Automatically determine if it's header-only based on presence of .cpp files
if(sources)
  # Normal library with source files.
  add_library(${TEMPLATE_LIBRARY_TARGET} ${headers} ${sources})
  target_compile_features(${TEMPLATE_LIBRARY_TARGET} PUBLIC cxx_std_23)

  set_target_properties(
    ${TEMPLATE_LIBRARY_TARGET}
    PROPERTIES CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF
  )

  target_include_directories(
    ${TEMPLATE_LIBRARY_TARGET}
    PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
  )

  # BUILD_INTERFACE keeps the policy targets out of any future export set.
  target_link_libraries(
    ${TEMPLATE_LIBRARY_TARGET}
    PUBLIC fmt::fmt
    PRIVATE $<BUILD_INTERFACE:myproj_warnings> $<BUILD_INTERFACE:myproj_options>
  )
else()
  # Header-only library.
  add_library(${TEMPLATE_LIBRARY_TARGET} INTERFACE)
  target_compile_features(${TEMPLATE_LIBRARY_TARGET} INTERFACE cxx_std_23)

  target_include_directories(
    ${TEMPLATE_LIBRARY_TARGET}
    INTERFACE $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
  )

  target_link_libraries(${TEMPLATE_LIBRARY_TARGET} INTERFACE fmt::fmt)
endif()

add_library(${TEMPLATE_NAMESPACED_TARGET} ALIAS ${TEMPLATE_LIBRARY_TARGET})

if(PROJECT_BUILD_STANDALONE)
  add_subdirectory(standalone)
endif()

if(PROJECT_BUILD_TESTS)
  enable_testing()
  add_subdirectory(test)
endif()
