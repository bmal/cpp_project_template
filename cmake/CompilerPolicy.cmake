# Compilation policy: the myproj_warnings and myproj_options interface targets.
# Every internal target links both PRIVATE, so nothing here reaches consumers.

option(
  PROJECT_WARNINGS_AS_ERRORS
  "Treat warnings as errors; applied only when this is the top-level project"
  ${PROJECT_IS_TOP_LEVEL}
)
option(PROJECT_STDLIB_HARDENING "Enable libstdc++ assertions and libc++ debug hardening" OFF)

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
target_compile_features(myproj_options INTERFACE cxx_std_23)
if(PROJECT_STDLIB_HARDENING)
  # Each macro is ignored by the other standard library, so both are always set.
  target_compile_definitions(
    myproj_options
    INTERFACE _GLIBCXX_ASSERTIONS _LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_DEBUG
  )
endif()

