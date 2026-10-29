# Add a dependency

Dependencies come from the vcpkg manifest and are built with the project's compiler and flags.

When you want a library, add its vcpkg port name to `dependencies` in `vcpkg.json`, then find it in the module's `CMakeLists.txt`:

```cmake
find_package(fmt CONFIG REQUIRED)
project_add_module(NAME core PRIVATE_DEPS fmt::fmt)
```

Use `PUBLIC_DEPS` when the dependency appears in the module's public headers, `PRIVATE_DEPS` otherwise.
The next configure installs it into `build/<preset>/vcpkg_installed/`:

```bash
make dev
```

When other projects use the installed package, add one line to `cmake/templates/Config.cmake.in`.
Modules are static, so consumers link the new package even when it is `PRIVATE`:

```cmake
find_dependency(fmt CONFIG)
```

When you want an existing vcpkg checkout instead of the bootstrapped `.vcpkg/`:

```bash
export VCPKG_ROOT=/path/to/vcpkg
```

The first configure of each preset builds dependencies and needs network access.
vcpkg caches the binaries in `~/.cache/vcpkg/archives`, so later presets reuse them.
