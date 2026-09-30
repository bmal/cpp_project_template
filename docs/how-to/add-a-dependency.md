# Add a dependency

Dependencies come from the vcpkg manifest and are built with the project's compiler and flags.

When you want a library, add its vcpkg port name to `dependencies` in `vcpkg.json`, then find it in the module's `CMakeLists.txt`:

```cmake
find_package(fmt CONFIG REQUIRED)
project_add_module(NAME core PUBLIC_DEPS fmt::fmt)
```

Use `PUBLIC_DEPS` when the dependency appears in the module's public headers, `PRIVATE_DEPS` otherwise.
The next configure installs it into `build/<preset>/vcpkg_installed/`:

```bash
make dev
```

When a module links the new package `PUBLIC`, consumers of the installed package need it too.
Add one line to `cmake/templates/Config.cmake.in`:

```cmake
find_dependency(fmt CONFIG)
```

When you want an existing vcpkg checkout instead of the bootstrapped `.vcpkg/`:

```bash
export VCPKG_ROOT=/path/to/vcpkg
```

The first configure of each preset builds dependencies and needs network access.
vcpkg caches the binaries in `~/.cache/vcpkg/archives`, so later presets reuse them.
