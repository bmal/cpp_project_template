# Switch to Conan

vcpkg is the shipped manager. Conan suits a team that already runs a Conan server.
The project's CMake calls only `find_package`, so the switch touches the toolchain file and the manifest, nothing else.

When you want Conan 2, install it and let it detect the compiler the presets use:

```bash
pipx install conan && CC=clang CXX=clang++ conan profile detect
```

Write `conanfile.txt` with the packages of `vcpkg.json`:

```ini
[requires]
fmt/[>=11 <12]
gtest/[>=1.15 <2]

[generators]
CMakeDeps
```

When you want the packages for a build, install them next to it, with the standard the project uses:

```bash
conan install . --output-folder=build/conan --build=missing -s compiler.cppstd=23
```

In `cmake/toolchain.cmake`, replace everything after the `compiler.cmake` include with one line:

```cmake
list(APPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_LIST_DIR}/../build/conan")
```

Then delete `vcpkg.json`, `triplets/`, and the vcpkg steps in `scripts/bootstrap.sh`, and run `make dev`.

Sanitizer presets lose their instrumented dependencies; give each one a Conan profile with the same flags.
