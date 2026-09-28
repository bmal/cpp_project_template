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
include("${_project_vcpkg_root}/scripts/buildsystems/vcpkg.cmake")
