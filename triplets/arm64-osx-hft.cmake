# Darwin arm64 triplet for release, profile, and bench: dependencies at -O3 for the project's CPU, with LTO
# when the preset has it. The toolchain exports PROJECT_MARCH_FLAG and PROJECT_LTO_FLAG.
set(VCPKG_TARGET_ARCHITECTURE arm64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CMAKE_SYSTEM_NAME Darwin)
set(VCPKG_OSX_ARCHITECTURES arm64)
set(VCPKG_BUILD_TYPE release)
set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE "${CMAKE_CURRENT_LIST_DIR}/../cmake/toolchain/compiler.cmake")
# Passed-through values are hashed into each package ABI, so a cached package never crosses CPUs.
set(
  VCPKG_ENV_PASSTHROUGH
  PROJECT_CXX_COMPILER
  PROJECT_C_COMPILER
  PROJECT_MARCH_FLAG
  PROJECT_LTO_FLAG
)
set(VCPKG_C_FLAGS "$ENV{PROJECT_MARCH_FLAG}")
set(VCPKG_CXX_FLAGS "$ENV{PROJECT_MARCH_FLAG}")
set(VCPKG_C_FLAGS_RELEASE "-O3 -DNDEBUG")
set(VCPKG_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
if(NOT "$ENV{PROJECT_LTO_FLAG}" STREQUAL "")
  # CMake archives LTO objects with the compiler's llvm-ar; the system ar cannot index them.
  list(
    APPEND VCPKG_CMAKE_CONFIGURE_OPTIONS
    -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON
    -DCMAKE_POLICY_DEFAULT_CMP0069=NEW
  )
endif()
