cmake_minimum_required(VERSION 3.20...3.30)

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

# ---- Include guards ----

if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
  message(
    FATAL_ERROR
      "In-source builds not allowed. Please make a new directory (called a build directory) and run CMake from there."
  )
endif()

# ---- Add dependencies via CPM ----
# see https://github.com/TheLartians/CPM.cmake for more info

include(cmake/CPM.cmake)
include(cmake/tools.cmake)

# PackageProject.cmake will be used to make our target installable
CPMAddPackage("gh:TheLartians/PackageProject.cmake@1.8.0")

CPMAddPackage(
  NAME fmt
  GIT_TAG 10.2.1
  GITHUB_REPOSITORY fmtlib/fmt
  OPTIONS "FMT_INSTALL YES" # create an installable target
)

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

  # Being a cross-platform target, we enforce standards conformance on MSVC.
  target_compile_options(${TEMPLATE_LIBRARY_TARGET} PUBLIC "$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/permissive->")

  target_include_directories(
    ${TEMPLATE_LIBRARY_TARGET}
    PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
           $<INSTALL_INTERFACE:include/${PROJECT_NAME}-${PROJECT_VERSION}>
  )

  target_link_libraries(${TEMPLATE_LIBRARY_TARGET} PUBLIC fmt::fmt)
else()
  # Header-only library.
  add_library(${TEMPLATE_LIBRARY_TARGET} INTERFACE)
  target_compile_features(${TEMPLATE_LIBRARY_TARGET} INTERFACE cxx_std_23)

  # Being a cross-platform target, we enforce standards conformance on MSVC.
  target_compile_options(${TEMPLATE_LIBRARY_TARGET} INTERFACE "$<$<COMPILE_LANG_AND_ID:CXX,MSVC>:/permissive->")

  target_include_directories(
    ${TEMPLATE_LIBRARY_TARGET}
    INTERFACE $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
              $<INSTALL_INTERFACE:include/${PROJECT_NAME}-${PROJECT_VERSION}>
  )

  target_link_libraries(${TEMPLATE_LIBRARY_TARGET} INTERFACE fmt::fmt)
endif()

# ---- Create an installable target ----
# this allows users to install and find the library via `find_package()`.

# the location where the project's version header will be placed should match the project's regular
# header paths
string(TOLOWER ${PROJECT_NAME}/version.h VERSION_HEADER_LOCATION)

packageProject(
  NAME ${TEMPLATE_PROJECT_NAME}
  VERSION ${PROJECT_VERSION}
  NAMESPACE ${TEMPLATE_NAMESPACE}
  BINARY_DIR ${PROJECT_BINARY_DIR}
  INCLUDE_DIR ${PROJECT_SOURCE_DIR}/include
  INCLUDE_DESTINATION include/${PROJECT_NAME}-${PROJECT_VERSION}
  VERSION_HEADER "${VERSION_HEADER_LOCATION}"
  COMPATIBILITY SameMajorVersion
  DEPENDENCIES "fmt 10.2.1"
)

if(PROJECT_BUILD_STANDALONE)
  add_subdirectory(standalone)
endif()

if(PROJECT_BUILD_TESTS)
  enable_testing()
  set(ENABLE_BENCHMARKS
      ${PROJECT_BUILD_BENCHMARKS}
      CACHE BOOL "Enable benchmark targets" FORCE
  )
  add_subdirectory(test)
endif()
