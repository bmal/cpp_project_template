# Compiler file: picks the compiler, enforces the version floor, and fixes the Homebrew libc++ link.
# The project toolchain includes it and every triplet chainloads it, so dependencies match the project.

# vcpkg port builds receive the project's resolved compilers through these variables.
if(NOT CMAKE_CXX_COMPILER AND DEFINED ENV{PROJECT_CXX_COMPILER})
  set(CMAKE_CXX_COMPILER "$ENV{PROJECT_CXX_COMPILER}")
  set(CMAKE_C_COMPILER "$ENV{PROJECT_C_COMPILER}")
endif()

if(NOT CMAKE_CXX_COMPILER)
  if(NOT PROJECT_COMPILER)
    set(PROJECT_COMPILER "clang")
  endif()
  if(PROJECT_COMPILER STREQUAL "clang" AND CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
    execute_process(
      COMMAND brew --prefix llvm
      OUTPUT_VARIABLE _project_llvm_prefix
      OUTPUT_STRIP_TRAILING_WHITESPACE
      ERROR_QUIET
      RESULT_VARIABLE _project_brew_result
    )
    if(NOT _project_brew_result EQUAL 0 OR NOT EXISTS "${_project_llvm_prefix}/bin/clang++")
      message(FATAL_ERROR "Homebrew LLVM was not found; run scripts/bootstrap.sh from the repository root.")
    endif()
    set(CMAKE_CXX_COMPILER "${_project_llvm_prefix}/bin/clang++")
  elseif(PROJECT_COMPILER STREQUAL "clang" OR PROJECT_COMPILER STREQUAL "gcc")
    if(PROJECT_COMPILER STREQUAL "clang")
      set(_project_driver "clang++")
    else()
      set(_project_driver "g++")
    endif()
    # Prefer the newest versioned driver, then the unversioned one.
    set(_project_names "")
    foreach(_project_version RANGE 30 14 -1)
      list(APPEND _project_names "${_project_driver}-${_project_version}")
    endforeach()
    find_program(_project_cxx NAMES ${_project_names} ${_project_driver} NO_CACHE)
    if(NOT _project_cxx)
      message(FATAL_ERROR "No ${_project_driver} was found; run scripts/bootstrap.sh from the repository root.")
    endif()
    set(CMAKE_CXX_COMPILER "${_project_cxx}")
  else()
    message(FATAL_ERROR "PROJECT_COMPILER must be clang or gcc, not '${PROJECT_COMPILER}'.")
  endif()
endif()

# Resolve a bare name such as g++-14 so vcpkg port builds get the same absolute path.
if(NOT IS_ABSOLUTE "${CMAKE_CXX_COMPILER}")
  find_program(_project_cxx_path NAMES "${CMAKE_CXX_COMPILER}" NO_CACHE REQUIRED)
  set(CMAKE_CXX_COMPILER "${_project_cxx_path}")
endif()
if(NOT CMAKE_C_COMPILER)
  string(REGEX REPLACE "clang\\+\\+([^/]*)$" "clang\\1" CMAKE_C_COMPILER "${CMAKE_CXX_COMPILER}")
  string(REGEX REPLACE "g\\+\\+([^/]*)$" "gcc\\1" CMAKE_C_COMPILER "${CMAKE_C_COMPILER}")
endif()
set(ENV{PROJECT_CXX_COMPILER} "${CMAKE_CXX_COMPILER}")
set(ENV{PROJECT_C_COMPILER} "${CMAKE_C_COMPILER}")

# Floor: GCC 14, Clang 19. AppleClang is best-effort and not checked.
execute_process(
  COMMAND "${CMAKE_CXX_COMPILER}" --version
  OUTPUT_VARIABLE _project_version_text
  ERROR_QUIET
)
if(_project_version_text MATCHES "Apple clang")
  # Best-effort, see the decision register.
elseif(_project_version_text MATCHES "clang version ([0-9]+)\\.")
  if(CMAKE_MATCH_1 LESS 19)
    message(FATAL_ERROR "This project requires Clang 19 or newer, but ${CMAKE_CXX_COMPILER} is Clang ${CMAKE_MATCH_1}.")
  endif()
elseif(_project_version_text MATCHES "Free Software Foundation")
  execute_process(
    COMMAND "${CMAKE_CXX_COMPILER}" -dumpfullversion
    OUTPUT_VARIABLE _project_gcc_version
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
  if(_project_gcc_version VERSION_LESS 14)
    message(FATAL_ERROR "This project requires GCC 14 or newer, but ${CMAKE_CXX_COMPILER} is GCC ${_project_gcc_version}.")
  endif()
endif()

# Homebrew LLVM ships a newer libc++ than macOS; link and load that one instead of the system copy.
cmake_path(GET CMAKE_CXX_COMPILER PARENT_PATH _project_compiler_bin)
cmake_path(GET _project_compiler_bin PARENT_PATH _project_compiler_prefix)
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin" AND EXISTS "${_project_compiler_prefix}/lib/c++/libc++.dylib")
  set(_project_libcxx "${_project_compiler_prefix}/lib/c++")
  set(_project_unwind "${_project_compiler_prefix}/lib/unwind")
  # The headers' availability markup describes the system libc++, not the one linked here.
  set(CMAKE_CXX_FLAGS_INIT "-D_LIBCPP_DISABLE_AVAILABILITY")
  set(CMAKE_CXX_STANDARD_LINK_DIRECTORIES "${_project_libcxx};${_project_unwind}")
  set(CMAKE_CXX_STANDARD_LIBRARIES_INIT
      "-lunwind -Wl,-rpath,${_project_libcxx} -Wl,-rpath,${_project_unwind}"
  )
endif()
