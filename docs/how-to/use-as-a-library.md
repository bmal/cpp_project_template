# Use the project as a library

Every module that is not `INTERNAL` installs as `MyProj::<module>`. A consumer never gets tests, apps, or `-Werror`.

When you want the modules in another project, install the release build into a prefix:

```bash
make release && cmake --install build/release --prefix "$HOME/.local"
```

In the consumer's `CMakeLists.txt`:

```cmake
find_package(MyProj REQUIRED)
target_link_libraries(app PRIVATE MyProj::core)
```

Configure the consumer with this project's compiler file and dependencies; `<myproj>` is this checkout, `<triplet>` the directory under `vcpkg_installed/`:

```bash
cmake -S . -B build -G Ninja -DCMAKE_TOOLCHAIN_FILE=<myproj>/cmake/toolchain/compiler.cmake -DCMAKE_PREFIX_PATH="$HOME/.local;<myproj>/build/release/vcpkg_installed/<triplet>"
```

When the consumer builds the project from source instead, `FetchContent` or `add_subdirectory` works too:

```cmake
FetchContent_Declare(myproj GIT_REPOSITORY https://github.com/<owner>/myproj.git GIT_TAG v0.1.0)
FetchContent_MakeAvailable(myproj)
```

When you want a release archive, `build/release/package/MyProj-<version>-<system>.tar.gz`:

```bash
cpack --config build/release/CPackConfig.cmake
```

The version lives only in `project(VERSION)`. `<myproj/version.hpp>` gives `version_string`, `git_commit`, and `git_dirty`.
