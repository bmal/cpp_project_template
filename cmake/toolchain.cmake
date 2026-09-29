# The only toolchain file presets name: selects the compiler, locates vcpkg, and chainloads it.
# PROJECT_COMPILER picks clang (default) or gcc; VCPKG_ROOT overrides the bootstrapped .vcpkg/.

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

# A sanitizer preset sets PROJECT_TRIPLET_VARIANT, for example asan, to pick triplets/<host>-asan.cmake.
if(PROJECT_TRIPLET_VARIANT AND NOT VCPKG_TARGET_TRIPLET)
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
  set(VCPKG_TARGET_TRIPLET "${_project_arch}-${_project_os}-${PROJECT_TRIPLET_VARIANT}" CACHE STRING "")
endif()
include("${_project_vcpkg_root}/scripts/buildsystems/vcpkg.cmake")
