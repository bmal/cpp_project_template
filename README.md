# C++23 Project Template

This repository is a compact C++23 template for libraries that may be header-only, source-backed, or a mix of both. It builds the library, an optional standalone executable, GoogleTest functional tests, and Google Benchmark benchmarks through one CMake target model.

## Layout

```text
include/         Public headers
source/          Library implementation files
standalone/      Small executable that consumes the library
test/functional/ GoogleTest tests
test/benchmark/  Google Benchmark targets
cmake/           Shared CMake tooling
all/             Convenience entry point for IDEs
```

The library target is `Trading::Trading`. Tests, benchmarks, and standalone code all link that target instead of adding include paths or sources manually.

## Quick Start

```bash
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
./build/dev/standalone/trading_standalone
```

Run benchmarks:

```bash
cmake --preset benchmarks
cmake --build --preset benchmarks
./build/benchmarks/test/benchmark/TradingBenchmarks
```

Build release tests:

```bash
cmake --preset release
cmake --build --preset release
ctest --preset release
```

## Common Configurations

```bash
cmake --preset asan
cmake --build --preset asan
ctest --preset asan
```

```bash
cmake --preset coverage
cmake --build --preset coverage
ctest --preset coverage
```

The root build is controlled by these options:

```cmake
TRADING_BUILD_STANDALONE   # Build standalone executable
TRADING_BUILD_TESTS        # Build functional tests
TRADING_BUILD_BENCHMARKS   # Build benchmarks with tests
ENABLE_COVERAGE            # Enable coverage flags/report target
USE_SANITIZER              # Address, Undefined, Thread, etc.
```

## Header-Only And Source-Backed Code

Header-only code lives in `include/` and should mark function definitions `inline` where needed.

Source-backed code has declarations in `include/` and definitions in `source/`. If `source/*.cpp` is empty, the library is configured as an `INTERFACE` target. If implementation files exist, it is configured as a normal library. In both cases, consumers receive the same public target: `Trading::Trading`.

Public headers currently require C++23, and that requirement is exported through `target_compile_features(... cxx_std_23)`.

## Adding Dependencies

Add runtime/library dependencies in the root [CMakeLists.txt](CMakeLists.txt). Link them to `Trading` with the correct visibility:

```cmake
target_link_libraries(Trading PUBLIC fmt::fmt)
```

Use `PUBLIC` when the dependency appears in public headers, and `PRIVATE` when it is only used by `.cpp` files.

## Standalone, Tests, And Benchmarks

Standalone code belongs in `standalone/source/` and should not contain library implementations.

Functional tests belong in `test/functional/source/`.

Benchmarks belong in `test/benchmark/source/`. The benchmark executable has its own `BENCHMARK_MAIN()` in `main.cpp`; benchmark files should only register benchmark functions.

## Installable Package

The library is packaged with `PackageProject.cmake`, generating a `TradingConfig.cmake` and exported `Trading::Trading` target. To validate installed-package use, configure the tests with:

```bash
cmake -S test -B build/test-installed -DTEST_INSTALLED_VERSION=ON -DCMAKE_PREFIX_PATH=/path/to/install
cmake --build build/test-installed
ctest --test-dir build/test-installed --output-on-failure
```

## Notes For New Projects

Rename `Trading` and `trading` consistently when deriving a new project from this template. The important contract to preserve is simple: one library target, `Project::Project`, consumed by every executable, test, benchmark, and downstream package.
