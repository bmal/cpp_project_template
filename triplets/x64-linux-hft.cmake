# Linux x64 triplet for release, profile, and bench: dependencies at -O3 for the project's CPU, with LTO
# when the preset has it. The toolchain exports PROJECT_MARCH_FLAG and PROJECT_LTO_FLAG.
set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CMAKE_SYSTEM_NAME Linux)
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
  # Fat objects keep machine code beside the LTO code, so ar indexes them and a consumer without LTO links.
  string(APPEND VCPKG_C_FLAGS " $ENV{PROJECT_LTO_FLAG} -ffat-lto-objects")
  string(APPEND VCPKG_CXX_FLAGS " $ENV{PROJECT_LTO_FLAG} -ffat-lto-objects")
endif()
