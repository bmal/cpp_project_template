# C++23 Library Project Template

A compact, modern C++23 template for projects that start small and still grow cleanly. A module is a directory under `libs/`, an app is a directory under `apps/`, and each takes one CMake call.

## What You Get

- `project_add_module` and `project_add_app`: one call per directory, compiled or header-only.
- Unit tests under `tests/unit/<module>` attach to their module automatically.
- C++23 enforced for every module, app, test, and benchmark.
- One strict warning set with `-Werror`, applied to every internal target and never to consumers.
- Every executable lands in `build/<preset>/bin/`.
- Dependencies from a vcpkg manifest, consumed only through `find_package`.
- One toolchain file that picks the compiler and builds dependencies with it.
- CMake presets for development, release, benchmarks, sanitizers, and coverage.
- VS Code tasks and debug launch configs that do not depend on the project name.
- GitHub Actions for normal build/test, coverage, and sanitizer checks, with heavier benchmark/perf workflows kept opt-in.

## Requirements

- macOS with Homebrew, or apt-based Linux.
- Clang 19 or GCC 14 at least; configure stops with an error on older compilers.
- CMake 3.28 or newer and Ninja.

`scripts/bootstrap.sh` installs all of these, plus vcpkg pinned under `.vcpkg/`.

## Repository Layout

```text
libs/<module>/include/<module>/  Public headers of a module
libs/<module>/src/               Optional module sources; none makes it header-only
apps/<name>/                     Executables
tests/unit/<module>/             GoogleTest unit tests of one module
benchmarks/<module>/             Google Benchmark executables
cmake/                           Module helpers, compiler policy, install and packaging, the toolchain file
scripts/                         Bootstrap script
triplets/                        vcpkg triplets that build dependencies with the project compiler
vcpkg.json                       Dependency manifest
```

Placeholders are literal: `myproj` for namespaces, targets, and directories, `MyProj` for the CMake project.

## Quick Start

Install the toolchain and vcpkg once per machine:

```bash
scripts/bootstrap.sh
```

Configure, build, and test:

```bash
cmake --workflow --preset dev
```

Run the sample CLI after a `dev` build:

```bash
build/dev/bin/myproj_cli
```

## Add A Module

Create `libs/<name>/CMakeLists.txt` with one line; `libs/` picks up every subdirectory that has one:

```cmake
project_add_module(NAME net PUBLIC_DEPS myproj::core)
```

This creates `myproj_net` with alias `myproj::net` and public include directory `libs/net/include`.

| Option | Effect |
| --- | --- |
| `PUBLIC_DEPS a b` | Linked `PUBLIC`; use when the dependency appears in public headers. |
| `PRIVATE_DEPS a b` | Linked `PRIVATE`; needs sources under `src/`. |
| `SOURCES a.cpp` | Replaces the default glob of `src/*.cpp`. |
| `NO_EXCEPTIONS` | Compiles the module sources with `-fno-exceptions`. |
| `NO_RTTI` | Compiles the module sources with `-fno-rtti`. |
| `INTERNAL` | Marks the module as not part of the installed package. |

No `.cpp` under `src/` makes an `INTERFACE` library. New files under `src/` are picked up by the next build without reconfiguring.

If `tests/unit/<name>/` exists, its `.cpp` files become `<name>_unit_tests`, labeled `unit` and `<name>`. A `tests/unit/` directory without a matching module fails configure.

## Add An App

Create `apps/<name>/CMakeLists.txt`; every `.cpp` under `apps/<name>/` is compiled:

```cmake
project_add_app(NAME myproj_tool DEPS myproj::core)
```

## Build Configurations

List every preset with its description:

```bash
cmake --list-presets=all
```

Configure, build, and test presets share a name, and each test preset has a workflow preset. The `-gcc` presets exist on Linux only. Build directories are `build/<preset>`.

Use an existing vcpkg checkout instead of `.vcpkg/`:

```bash
export VCPKG_ROOT=/path/to/vcpkg
```

Root project options:

```cmake
PROJECT_COMPILER           # clang (default) or gcc
PROJECT_BUILD_APPS         # Build apps/; ON when top level
PROJECT_BUILD_TESTS        # Build tests/; ON when top level
PROJECT_BUILD_BENCHMARKS   # Build benchmarks/
PROJECT_SANITIZE           # Value for -fsanitize=, for example address,undefined
PROJECT_WARNINGS_AS_ERRORS # -Werror; ON when top level, never applied to consumers
PROJECT_STDLIB_HARDENING   # libstdc++ assertions and libc++ debug hardening; ON in dev
ENABLE_COVERAGE            # Enable coverage flags for tests
```

Warnings and options come from the `myproj_warnings` and `myproj_options` interface targets in [cmake/CompilerPolicy.cmake](cmake/CompilerPolicy.cmake). The module and app helpers link both `PRIVATE`.

When a compiler upgrade raises a warning you cannot fix yet, build without `-Werror`:

```bash
cmake --preset dev -DPROJECT_WARNINGS_AS_ERRORS=OFF
```

## Adding Dependencies

Add the vcpkg port name to `dependencies` in [vcpkg.json](vcpkg.json), then find it in CMake:

```cmake
find_package(fmt CONFIG REQUIRED)
```

The next configure installs it into `build/<preset>/vcpkg_installed/`.

Link with the narrowest correct visibility:

```cmake
project_add_module(NAME core PUBLIC_DEPS fmt::fmt)
```

## Tests

Run every test of the `dev` preset:

```bash
ctest --preset dev
```

Run the unit tests of one module:

```bash
ctest --preset dev -L core
```

## Install And Package

Install the release build into a prefix, to use the modules from another project:

```bash
cmake --workflow --preset release
cmake --install build/release --prefix "$HOME/.local"
```

Consume it from another CMake project; each module that is not `INTERNAL` is `MyProj::<module>`:

```cmake
find_package(MyProj REQUIRED)
target_link_libraries(app PRIVATE MyProj::core)
```

Build a release archive, written to `build/release/package/MyProj-<version>-<system>.tar.gz`:

```bash
cpack --config build/release/CPackConfig.cmake
```

The version lives only in `project(VERSION)` in [CMakeLists.txt](CMakeLists.txt). `#include <myproj/version.hpp>` from `myproj::core` gives `version_string`, `git_commit`, and `git_dirty`, captured at configure time.

When a module links a new package `PUBLIC`, add its `find_dependency` to [cmake/templates/Config.cmake.in](cmake/templates/Config.cmake.in).

## Benchmarks

Benchmarks live in `benchmarks/<module>`. Build and run them:

```bash
cmake --preset benchmarks
cmake --build --preset benchmarks
build/benchmarks/bin/core_benchmarks
```

Use benchmark numbers from local laptops, virtual machines, and hosted CI as rough signals only. For meaningful latency work, especially HFT-style systems, prefer a controlled Linux machine with fixed toolchains, release builds, CPU governor/perf configured, isolated cores, and repeatable measurement scripts.

## VS Code Workflow

Included VS Code files provide:

- Configure/build/test tasks.
- Benchmark and CLI run tasks.
- Debug launch configs for `build/dev/bin/core_unit_tests` and `build/dev/bin/myproj_cli`.
- clangd configured to read compile commands from `build/dev`.

Start with:

```bash
cmake --preset dev
```

That generates `build/dev/compile_commands.json`, which clangd uses for accurate indexing.

## CI

The default GitHub Actions pipeline is intentionally lean:

- Build and run tests with the `dev`, `release`, `dev-gcc`, and `release-gcc` workflow presets.
- Generate coverage.
- Run sanitizer checks.

Codecov upload is enabled when the repository secret `CODECOV_TOKEN` is configured. Without that secret, CI still generates and uploads the coverage report artifact, but skips the external Codecov upload to avoid tokenless rate-limit failures.

Benchmarks, valgrind, cachegrind, and broader performance workflows remain available as opt-in workflows. This keeps ordinary pull requests fast while preserving tooling for performance-critical projects.

Linux runners are pinned to explicit Ubuntu versions instead of floating `ubuntu-latest` to reduce surprise toolchain changes.

## Platform Notes

macOS is supported for everyday development: configure, build, tests, app runs, coverage, and basic benchmark smoke checks. Do not treat macOS benchmark CPU metadata or affinity behavior as authoritative for final latency claims.

Linux is the recommended platform for performance-sensitive work. For HFT-style benchmarking, use pinned compilers, `Release` or `RelWithDebInfo`, CPU governor/perf permissions, isolated cores, and preferably self-hosted runners or dedicated hardware.

## Common Tasks

Run sanitizer checks locally:

```bash
cmake --workflow --preset asan
```

Generate coverage flags locally:

```bash
cmake --workflow --preset coverage
```

## Troubleshooting

If clangd shows old commands, regenerate the dev preset:

```bash
cmake --preset dev
```

If configure says vcpkg was not found, install it:

```bash
scripts/bootstrap.sh
```

The first configure of each preset builds dependencies and needs network access. vcpkg caches the binaries in `~/.cache/vcpkg/archives`, so later presets reuse them.
