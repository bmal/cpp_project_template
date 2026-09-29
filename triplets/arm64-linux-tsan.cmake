# Linux arm64 triplet for the tsan preset: static dependencies instrumented with ThreadSanitizer.
# The project's compiler file builds them and applies the VCPKG_*_FLAGS below.
set(VCPKG_TARGET_ARCHITECTURE arm64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CMAKE_SYSTEM_NAME Linux)
set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE "${CMAKE_CURRENT_LIST_DIR}/../cmake/toolchain/compiler.cmake")
set(VCPKG_ENV_PASSTHROUGH PROJECT_CXX_COMPILER PROJECT_C_COMPILER)
set(VCPKG_C_FLAGS "-fsanitize=thread -fno-omit-frame-pointer")
set(VCPKG_CXX_FLAGS "-fsanitize=thread -fno-omit-frame-pointer")
set(VCPKG_LINKER_FLAGS "-fsanitize=thread")
