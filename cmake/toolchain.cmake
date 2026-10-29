# The only toolchain file presets name: selects the compiler, picks the triplet, and chainloads vcpkg.
# PROJECT_COMPILER picks clang (default) or gcc; VCPKG_ROOT overrides the bootstrapped .vcpkg/. The hft
# triplet reads the CPU and LTO flags exported here.

include("${CMAKE_CURRENT_LIST_DIR}/toolchain/compiler.cmake")

cmake_path(GET CMAKE_CURRENT_LIST_DIR PARENT_PATH _project_root)

if(DEFINED ENV{VCPKG_ROOT} AND EXISTS "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
  set(_project_vcpkg_root "$ENV{VCPKG_ROOT}")
elseif(EXISTS "${_project_root}/.vcpkg/scripts/buildsystems/vcpkg.cmake")
  set(_project_vcpkg_root "${_project_root}/.vcpkg")
else()
  message(
    FATAL_ERROR
    "vcpkg was not found in VCPKG_ROOT or .vcpkg/; install it with:\n  scripts/bootstrap.sh\n"
  )
endif()

set(VCPKG_OVERLAY_TRIPLETS "${_project_root}/triplets" CACHE STRING "")

if(CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64|ARM64)$")
  set(_project_arch arm64)
else()
  set(_project_arch x64)
endif()
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
  set(_project_os osx)
else()
  set(_project_os linux)
endif()

# The hft triplet reads these. native becomes the CPU it names here, so the package ABI hashes the CPU.
set(ENV{PROJECT_MARCH_FLAG} "")
if(NOT PROJECT_TRIPLET_VARIANT STREQUAL "hft")
  # Only the hft triplet reads the flag, so other presets never run the probe below.
elseif(PROJECT_MARCH STREQUAL "native")
  if(_project_version_text MATCHES "Free Software Foundation")
    execute_process(
      COMMAND "${CMAKE_CXX_COMPILER}" -march=native -E -v -x c++ /dev/null
      ERROR_VARIABLE _project_driver_output
      OUTPUT_QUIET
    )
    # The cc1plus line carries the expanded value; the COLLECT_GCC_OPTIONS line still says native.
    string(REGEX MATCH "cc1plus[^\n]* -march=([^ \n]+)" _project_cpu "${_project_driver_output}")
    set(_project_cpu "${CMAKE_MATCH_1}")
    set(_project_flag "-march=${_project_cpu}")
  else()
    execute_process(
      COMMAND "${CMAKE_CXX_COMPILER}" -march=native "-###" -E -x c++ /dev/null
      ERROR_VARIABLE _project_driver_output
      OUTPUT_QUIET
    )
    string(REGEX MATCH "\"-target-cpu\" \"([^\"]+)\"" _project_cpu "${_project_driver_output}")
    set(_project_cpu "${CMAKE_MATCH_1}")
    # Clang names an Arm CPU only through -mcpu; -march takes an architecture there.
    if(_project_arch STREQUAL "arm64")
      set(_project_flag "-mcpu=${_project_cpu}")
    else()
      set(_project_flag "-march=${_project_cpu}")
    endif()
  endif()
  if(NOT _project_cpu OR _project_cpu STREQUAL "native")
    message(
      FATAL_ERROR
      "${CMAKE_CXX_COMPILER} did not say which CPU -march=native means; configure with an explicit PROJECT_MARCH such as x86-64-v3."
    )
  endif()
  set(ENV{PROJECT_MARCH_FLAG} "${_project_flag}")
  # CompilerPolicy reads this, not the environment, which each try_compile clears by rereading this file.
  set(PROJECT_NATIVE_FLAG "${_project_flag}")
elseif(PROJECT_MARCH)
  set(ENV{PROJECT_MARCH_FLAG} "-march=${PROJECT_MARCH}")
endif()
# Mirrors the flag CMake's LTO gives the project, thin for Clang: a thin module cannot import from a full one.
if(NOT CMAKE_INTERPROCEDURAL_OPTIMIZATION)
  set(ENV{PROJECT_LTO_FLAG} "")
elseif(_project_version_text MATCHES "Free Software Foundation")
  set(ENV{PROJECT_LTO_FLAG} "-flto=auto")
else()
  set(ENV{PROJECT_LTO_FLAG} "-flto=thin")
endif()

# A preset sets PROJECT_TRIPLET_VARIANT, for example hft or asan, to pick triplets/<host>-<variant>.cmake.
if(PROJECT_TRIPLET_VARIANT)
  set(_project_triplet "${_project_arch}-${_project_os}-${PROJECT_TRIPLET_VARIANT}")
  if(NOT EXISTS "${_project_root}/triplets/${_project_triplet}.cmake")
    message(
      FATAL_ERROR
      "PROJECT_TRIPLET_VARIANT=${PROJECT_TRIPLET_VARIANT} needs triplets/${_project_triplet}.cmake, which does not exist."
    )
  endif()
  # find_package results in the cache point into the old triplet's tree, so only a fresh configure switches.
  if(VCPKG_TARGET_TRIPLET AND NOT VCPKG_TARGET_TRIPLET STREQUAL _project_triplet)
    message(
      FATAL_ERROR
      "This build directory uses the ${VCPKG_TARGET_TRIPLET} triplet, not ${_project_triplet}; configure it again with --fresh."
    )
  endif()
  set(VCPKG_TARGET_TRIPLET "${_project_triplet}" CACHE STRING "")
endif()
include("${_project_vcpkg_root}/scripts/buildsystems/vcpkg.cmake")
