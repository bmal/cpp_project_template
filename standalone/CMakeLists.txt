cmake_minimum_required(VERSION 3.14...3.22)

project(TradingStandalone LANGUAGES CXX)

# --- Import tools ----
include(../cmake/tools.cmake)

# ---- Dependencies ----
include(../cmake/CPM.cmake)

if(PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
  message(
    FATAL_ERROR
      "In-source builds not allowed. Please make a new directory (called a build directory) and run CMake from there."
  )
endif()

CPMAddPackage(
    NAME Trading
    SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/..
)

# ---- Create standalone executable ----
file(GLOB_RECURSE sources CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp)
add_executable(trading_standalone ${sources})
target_link_libraries(trading_standalone PRIVATE Trading::Trading)

target_include_directories(trading_standalone PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../include)

target_compile_features(trading_standalone PRIVATE cxx_std_23)
set_target_properties(trading_standalone PROPERTIES 
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# ---- Enable warnings ----
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    target_compile_options(trading_standalone PRIVATE -Wall -Wpedantic -Wextra -Werror)
elseif(MSVC)
    target_compile_options(trading_standalone PRIVATE /W4 /WX)
endif()
