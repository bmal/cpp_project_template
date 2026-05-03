# this script adds all subprojects to a single build to allow IDEs understand the full project
# structure.

cmake_minimum_required(VERSION 3.20...3.30)

project(BuildAll LANGUAGES CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(../cmake/tools.cmake)

# needed to generate test target
enable_testing()

set(TRADING_BUILD_STANDALONE ON CACHE BOOL "" FORCE)
set(TRADING_BUILD_TESTS ON CACHE BOOL "" FORCE)
set(TRADING_BUILD_BENCHMARKS ON CACHE BOOL "" FORCE)

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/.. ${CMAKE_BINARY_DIR}/Trading)
