# this script adds all subprojects to a single build to allow IDEs understand the full project
# structure.

cmake_minimum_required(VERSION 3.20...3.30)

include(../cmake/ProjectIdentity.cmake)

project(BuildAll LANGUAGES CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(../cmake/tools.cmake)

# needed to generate test target
enable_testing()

set(PROJECT_BUILD_STANDALONE ON CACHE BOOL "" FORCE)
set(PROJECT_BUILD_TESTS ON CACHE BOOL "" FORCE)
set(PROJECT_BUILD_BENCHMARKS ON CACHE BOOL "" FORCE)

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/.. ${CMAKE_BINARY_DIR}/${TEMPLATE_BINARY_DIR_NAME})
