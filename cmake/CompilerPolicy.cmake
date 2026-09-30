# Compilation policy: the myproj_warnings and myproj_options interface targets.
# Every internal target links both PRIVATE, so nothing here reaches consumers.

option(
  PROJECT_WARNINGS_AS_ERRORS
  "Treat warnings as errors; applied only when this is the top-level project"
  ${PROJECT_IS_TOP_LEVEL}
)
option(PROJECT_STDLIB_HARDENING "Enable libstdc++ assertions and libc++ debug hardening" OFF)
option(PROJECT_FRAME_POINTERS "Keep frame pointers so profilers can unwind optimized code" OFF)
set(PROJECT_CXX_STANDARD 23 CACHE STRING "C++ standard of every internal target: 23 or 26")
set_property(CACHE PROJECT_CXX_STANDARD PROPERTY STRINGS 23 26)
if(NOT PROJECT_CXX_STANDARD MATCHES "^(23|26)$")
  message(FATAL_ERROR "PROJECT_CXX_STANDARD must be 23 or 26, not '${PROJECT_CXX_STANDARD}'.")
endif()
# Empty means the compiler's default target; release sets native, which binds the binary to this CPU.
set(
  PROJECT_MARCH
  ""
  CACHE STRING
  "Value of -march for internal targets, such as native or x86-64-v3"
)

add_library(myproj_warnings INTERFACE)
target_compile_options(
  myproj_warnings
  INTERFACE
    -Wall
    -Wextra
    -Wpedantic
    -Wshadow
    -Wconversion
    -Wsign-conversion
    -Wnon-virtual-dtor
    -Wold-style-cast
    -Wcast-align
    -Woverloaded-virtual
    -Wnull-dereference
    -Wdouble-promotion
    -Wformat=2
    -Wimplicit-fallthrough
    $<$<CXX_COMPILER_ID:GNU>:-Wduplicated-cond;-Wduplicated-branches;-Wlogical-op;-Wuseless-cast>
    $<$<CXX_COMPILER_ID:Clang,AppleClang>:-Wextra-semi;-Wunreachable-code>
)
if(PROJECT_IS_TOP_LEVEL AND PROJECT_WARNINGS_AS_ERRORS)
  target_compile_options(myproj_warnings INTERFACE -Werror)
endif()

add_library(myproj_options INTERFACE)
if("cxx_std_${PROJECT_CXX_STANDARD}" IN_LIST CMAKE_CXX_COMPILE_FEATURES)
  target_compile_features(myproj_options INTERFACE cxx_std_${PROJECT_CXX_STANDARD})
else()
  # CMake before 3.30 has no cxx_std_26 for GCC or Clang; this flag follows, and overrides, -std=c++23.
  target_compile_options(myproj_options INTERFACE -std=c++${PROJECT_CXX_STANDARD})
endif()
# Clang drops standard library type information by default, so the debugger shows incomplete types.
target_compile_options(
  myproj_options
  INTERFACE $<$<AND:$<CONFIG:Debug>,$<CXX_COMPILER_ID:Clang,AppleClang>>:-fstandalone-debug>
)
if(PROJECT_MARCH)
  target_compile_options(myproj_options INTERFACE -march=${PROJECT_MARCH})
endif()
# An installed archive keeps machine code beside the LTO bitcode, so a consumer without LTO links it.
if(CMAKE_INTERPROCEDURAL_OPTIMIZATION AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
  target_compile_options(myproj_options INTERFACE -ffat-lto-objects)
endif()
if(PROJECT_STDLIB_HARDENING)
  # Each macro is ignored by the other standard library, so both are always set.
  target_compile_definitions(
    myproj_options
    INTERFACE _GLIBCXX_ASSERTIONS _LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_DEBUG
  )
endif()
if(PROJECT_FRAME_POINTERS)
  target_compile_options(myproj_options INTERFACE -fno-omit-frame-pointer)
endif()
