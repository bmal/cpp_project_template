# C++23 Library Project Template

A compact, modern C++23 template for library projects that can start small and still grow cleanly. It supports header-only libraries, source-backed libraries, standalone examples, GoogleTest functional tests, Google Benchmark benchmarks, installable CMake packages, VS Code workflows, and lean CI.

## What You Get

- One CMake library target that works as either a normal compiled library or a header-only `INTERFACE` library.
- C++23 enforced for the library, standalone executable, tests, and benchmarks.
- A single project identity knob that derives target, package, test, benchmark, and executable names.
- GoogleTest functional tests and Google Benchmark benchmarks.
- Stable local launcher paths: `build/<preset>/bin/tests`, `build/<preset>/bin/standalone`, and `build/<preset>/bin/benchmarks`.
- Install/export support through `PackageProject.cmake`, so consumers can use `find_package(...)`.
- CMake presets for development, release, benchmarks, sanitizers, and coverage.
- VS Code tasks and debug launch configs that do not depend on the project name.
- GitHub Actions for normal build/test, coverage, and sanitizer checks, with heavier benchmark/perf workflows kept opt-in.

## Requirements

- CMake 3.20 or newer.
- A compiler with C++23 support.
- Git, because dependencies are fetched through CPM.
- Optional local tools:
  - `ccache` for faster rebuilds.
  - `lcov` and `genhtml` for GCC coverage reports.
  - VS Code with clangd if you want the included editor workflow.

Dependencies such as fmt, GoogleTest, Google Benchmark, PackageProject, and optional tooling are fetched by CMake/CPM during configuration.

## Repository Layout

```text
include/           Public headers installed for consumers
source/            Optional library implementation files
standalone/        Small executable that consumes the library
test/functional/   GoogleTest test executable
test/benchmark/    Google Benchmark executable
cmake/             Shared CMake modules
all/               IDE-oriented build that includes library, tests, benchmarks, and standalone
```

The current sample API uses `include/module` and namespace `Example`. Treat both as neutral placeholder code. Rename them when you create a real library domain.

## Quick Start

Configure, build, test, and run the standalone executable:

```bash
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
cmake --build --preset dev --target run-standalone
```

Build and run benchmarks:

```bash
cmake --preset benchmarks
cmake --build --preset benchmarks
cmake --build --preset benchmarks --target run-benchmarks
```

Build release mode:

```bash
cmake --preset release
cmake --build --preset release
ctest --preset release
```

## Rename The Template

The project identity lives in one place: [cmake/ProjectIdentity.cmake](cmake/ProjectIdentity.cmake).

```cmake
set(TEMPLATE_PROJECT_NAME
    "DummyProject"
    CACHE STRING "Project name used to derive template target and package names"
)
```

Change `"DummyProject"` to your project name, then configure a fresh build directory. CMake cache variables intentionally persist, so an existing build directory may keep the old name until it is recreated.

The name must match this simple CMake-friendly pattern:

```text
[A-Za-z][A-Za-z0-9_]*
```

Derived names are created automatically:

```text
Library target:        MyProject
Namespaced target:    MyProject::MyProject
Test target:          MyProjectTests
Benchmark target:     MyProjectBenchmarks
Standalone target:    myproject_standalone
Package name:         MyProject
```

After renaming, search for placeholders you probably want to replace:

```bash
rg "DummyProject|Example|include/module|module/"
```

## Build Configurations

Presets are defined in [CMakePresets.json](CMakePresets.json).

```bash
cmake --preset dev          # Debug library, tests, standalone
cmake --preset release      # Release library, tests, standalone
cmake --preset benchmarks   # Release tests plus benchmarks
cmake --preset asan         # Address/Undefined sanitizer testing
cmake --preset coverage     # Debug coverage build
```

Build and test presets share the same names:

```bash
cmake --build --preset dev
ctest --preset dev
```

Root project options:

```cmake
PROJECT_BUILD_STANDALONE   # Build standalone/
PROJECT_BUILD_TESTS        # Build test/
PROJECT_BUILD_BENCHMARKS   # Build test/benchmark/ when tests are enabled
ENABLE_COVERAGE            # Enable coverage flags for tests
USE_SANITIZER              # Address, Undefined, Thread, etc.
TEST_INSTALLED_VERSION     # Test an installed package via find_package
```

## Header-Only And Source-Backed Modes

Headers live under `include/`. Implementation files live under `source/`.

If `source/*.cpp` exists, CMake creates a normal compiled library:

```cmake
add_library(${TEMPLATE_LIBRARY_TARGET} ${headers} ${sources})
```

If no implementation files exist, CMake creates a header-only `INTERFACE` library:

```cmake
add_library(${TEMPLATE_LIBRARY_TARGET} INTERFACE)
```

Consumers link the same public target in both cases:

```cmake
target_link_libraries(my_app PRIVATE ${TEMPLATE_PROJECT_NAME}::${TEMPLATE_PROJECT_NAME})
```

Header-only function definitions should be `inline` when needed. Source-backed APIs should put declarations in `include/` and definitions in `source/`.

## Adding Dependencies

Add library dependencies in the root [CMakeLists.txt](CMakeLists.txt), usually with CPM:

```cmake
CPMAddPackage(
  NAME fmt
  GIT_TAG 10.2.1
  GITHUB_REPOSITORY fmtlib/fmt
  OPTIONS "FMT_INSTALL YES"
)
```

Link with the narrowest correct visibility:

```cmake
target_link_libraries(${TEMPLATE_LIBRARY_TARGET} PUBLIC fmt::fmt)
```

Use:

- `PUBLIC` when the dependency appears in public headers or is part of the consumer contract.
- `PRIVATE` when the dependency is used only by `.cpp` files.
- `INTERFACE` for usage requirements of header-only targets.

If a dependency must be available to installed-package consumers, include it in the package dependency list as well.

## Tests

Functional tests live in `test/functional/source` and are discovered with `gtest_discover_tests`.

Run tests from the root preset workflow:

```bash
ctest --preset dev
```

Or build tests as a standalone subproject:

```bash
cmake -S test -B build/test -DENABLE_BENCHMARKS=OFF
cmake --build build/test
ctest --test-dir build/test --output-on-failure
```

The test executable is copied to a stable path:

```text
build/<config>/bin/tests
```

That path is used by VS Code and CI so renaming the project does not require editing launch configs.

## Benchmarks

Benchmarks live in `test/benchmark/source`.

```bash
cmake --preset benchmarks
cmake --build --preset benchmarks
cmake --build --preset benchmarks --target run-benchmarks
```

For a smoke check without running measurements:

```bash
./build/benchmarks/bin/benchmarks --benchmark_list_tests=true
```

Use benchmark numbers from local laptops, virtual machines, and hosted CI as rough signals only. For meaningful latency work, especially HFT-style systems, prefer a controlled Linux machine with fixed toolchains, release builds, CPU governor/perf configured, isolated cores, and repeatable measurement scripts.

## Install And Package

The root project uses `PackageProject.cmake` to generate an installable CMake package and exported namespaced target.

Install locally:

```bash
cmake --preset dev
cmake --build --preset dev
cmake --install build/dev --prefix build/install-smoke
```

Verify that consumers can use the installed package:

```bash
cmake -S test -B build/test-installed \
  -DTEST_INSTALLED_VERSION=ON \
  -DCMAKE_PREFIX_PATH="$PWD/build/install-smoke"
cmake --build build/test-installed
ctest --test-dir build/test-installed --output-on-failure
```

## VS Code Workflow

Included VS Code files provide:

- Configure/build/test tasks.
- Benchmark and standalone run tasks.
- Debug launch configs for `build/dev/bin/tests` and `build/dev/bin/standalone`.
- clangd configured to read compile commands from `build/dev`.

Start with:

```bash
cmake --preset dev
```

That generates `build/dev/compile_commands.json`, which clangd uses for accurate indexing.

## CI

The default GitHub Actions pipeline is intentionally lean:

- Build and run tests.
- Generate coverage.
- Run sanitizer checks.

Benchmarks, valgrind, cachegrind, and broader performance workflows remain available as opt-in workflows. This keeps ordinary pull requests fast while preserving tooling for performance-critical projects.

Linux runners are pinned to explicit Ubuntu versions instead of floating `ubuntu-latest` to reduce surprise toolchain changes.

## Platform Notes

macOS is supported for everyday development: configure, build, tests, standalone runs, coverage, and basic benchmark smoke checks. Do not treat macOS benchmark CPU metadata or affinity behavior as authoritative for final latency claims.

Linux is the recommended platform for performance-sensitive work. For HFT-style benchmarking, use pinned compilers, `Release` or `RelWithDebInfo`, CPU governor/perf permissions, isolated cores, and preferably self-hosted runners or dedicated hardware.

## Common Tasks

Create a new source-backed component:

```text
include/module/my_component.hpp
source/my_component.cpp
test/functional/source/test_my_component.cpp
```

Create a header-only component:

```text
include/module/my_header_only_component.hpp
test/functional/source/test_my_header_only_component.cpp
```

Add a benchmark:

```text
test/benchmark/source/benchmark_my_component.cpp
```

Run sanitizer checks locally:

```bash
cmake --preset asan
cmake --build --preset asan
ctest --preset asan
```

Generate coverage flags locally:

```bash
cmake --preset coverage
cmake --build --preset coverage
ctest --preset coverage
```

## Troubleshooting

If renamed targets still look stale, use a fresh build directory. CMake caches project variables, and old executable files are not removed automatically.

If clangd shows old commands, regenerate the dev preset:

```bash
cmake --preset dev
```

If a fresh build cannot configure, check network access. CPM downloads dependencies during configuration unless they are already cached.

If installed-package tests fail, confirm `CMAKE_PREFIX_PATH` points to the same prefix passed to `cmake --install`.
