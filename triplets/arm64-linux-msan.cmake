# Linux arm64 triplet for the msan preset: static dependencies instrumented with MemorySanitizer.
# The project's compiler file builds them against the instrumented libc++ it names in PROJECT_MSAN_LIBCXX.
set(VCPKG_TARGET_ARCHITECTURE arm64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CMAKE_SYSTEM_NAME Linux)
set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE "${CMAKE_CURRENT_LIST_DIR}/../cmake/toolchain/compiler.cmake")
set(VCPKG_ENV_PASSTHROUGH PROJECT_CXX_COMPILER PROJECT_C_COMPILER PROJECT_MSAN_LIBCXX)
set(VCPKG_C_FLAGS "-fsanitize=memory -fsanitize-memory-track-origins=2 -fno-omit-frame-pointer")
set(VCPKG_CXX_FLAGS "-fsanitize=memory -fsanitize-memory-track-origins=2 -fno-omit-frame-pointer")
set(VCPKG_LINKER_FLAGS "-fsanitize=memory")
