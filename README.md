# C++23 Library Project Template

A compact, modern C++23 template for projects that start small and still grow cleanly. A module is a directory under `libs/`, an app is a directory under `apps/`, and each takes one CMake call.

## What You Get

- `project_add_module` and `project_add_app`: one call per directory, compiled or header-only.
- Unit tests under `tests/unit/<module>` attach to their module automatically.
- C++23 for every module, app, test, and benchmark; the `cxx26` preset builds the same code as C++26.
- One strict warning set with `-Werror`, applied to every internal target and never to consumers.
- Every executable lands in `build/<preset>/bin/`.
- Dependencies from a vcpkg manifest, consumed only through `find_package`.
- One toolchain file that picks the compiler and builds dependencies with it.
- CMake presets for development, release, benchmarks, sanitizers, fuzzing, and coverage.
- VS Code settings, debug launches, and tasks that follow the last configured preset.
- One GitHub Actions pipeline that runs the same presets, reporting through one `status` check.

## Requirements

- macOS with Homebrew, or apt-based Linux.
- Clang 19 or GCC 14 at least; configure stops with an error on older compilers.
- CMake 3.28 or newer, Ninja, and Make.

`scripts/bootstrap.sh` installs all of these, plus vcpkg pinned under `.vcpkg/` and the pre-commit hook.
The root `Dockerfile` runs it too, for the Linux dev container.

## Repository Layout

```text
libs/<module>/include/<module>/  Public headers of a module
libs/<module>/src/               Optional module sources; none makes it header-only
apps/<name>/                     Executables
tests/unit/<module>/             GoogleTest unit tests of one module
tests/integration/               Tests that wire modules together, with a mock at the output edge
tests/functional/                Black-box tests that spawn the apps
tests/fuzz/                      libFuzzer harnesses, one executable each, with seed inputs under corpus/
tests/support/                    Builders, fake clock, allocation guard, skip macros, and the shared test main
tests/sanitizers/                Sanitizer suppression files, read by the asan and tsan test presets
benchmarks/<module>/             Google Benchmark executables, one per module
benchmarks/support/              The shared benchmark main, cache flush, and percentile helpers
cmake/                           Module helpers, compiler policy, install and packaging, the toolchain file
scripts/                         Bootstrap, init, scaffolding, benchmark, MSan libc++, CI image, and template selftest scripts
triplets/                        vcpkg triplets that build dependencies with the project compiler
vcpkg.json                       Dependency manifest
```

The sample code is small and each file opens with the mechanism it demonstrates: `libs/parser` is header-only and exception-free, `libs/core` is compiled and wires the parser to an output interface, and `apps/myproj_cli` prints through that interface.

## Name The Project

A repository created from this template names itself on its first push, from the repository name.
Until then the placeholders are literal: `myproj`, `MyProj`, and `MYPROJ`.

When the name it picked is wrong, or you copied the template by hand, rename in one command and commit:

```bash
scripts/init-project.sh order_book
```

When you want a project with only `core`, one app, and one passing test, drop the parser sample too:

```bash
scripts/init-project.sh order_book --strip-samples
```

## Protect The Repository

When the repository is on GitHub and its first push has named it, add the CI labels, require `status` on `main`, and allow only squash merges:

```bash
make setup-repo
```

It prints one line per change, or `no changes`. It needs `gh auth login` as an administrator of the repository.
Pushes to `main` then go through pull requests. The merge queue is added only to a public repository an organization owns, because GitHub offers it nowhere else outside Enterprise Cloud.

## Quick Start

Install the toolchain, `make`, and vcpkg once per machine:

```bash
scripts/bootstrap.sh
```

Configure, build, and test:

```bash
make dev
```

List every other common action:

```bash
make help
```

Run the sample CLI after a `dev` build; it echoes `key=value` fields and exits 1 on a malformed line:

```bash
echo "a=1 b=2" | build/dev/bin/myproj_cli
```

## Add A Module

Scaffold a compiled module `myproj::net` with a namespace stub and one passing unit test:

```bash
scripts/new-module.sh net
```

Or write `libs/<name>/CMakeLists.txt` by hand with one line; `libs/` picks up every subdirectory that has one:

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

Scaffold an executable `build/<preset>/bin/tool` that links `myproj::core`:

```bash
scripts/new-app.sh tool
```

Or write `apps/<name>/CMakeLists.txt` by hand; every `.cpp` under `apps/<name>/` is compiled:

```cmake
project_add_app(NAME myproj_tool DEPS myproj::core)
```

## Build Configurations

List every preset with its description:

```bash
cmake --list-presets=all
```

Configure, build, and test presets share a name, and each test preset has a workflow preset. The `-gcc` presets exist on Linux only. Build directories are `build/<preset>`.
Configure also points `build/current` and the root `compile_commands.json` at that directory. [docs/how-to/personal-presets.md](docs/how-to/personal-presets.md) adds a preset of your own.

Use an existing vcpkg checkout instead of `.vcpkg/`:

```bash
export VCPKG_ROOT=/path/to/vcpkg
```

Root project options:

```cmake
PROJECT_COMPILER           # clang (default) or gcc
PROJECT_CXX_STANDARD       # 23 (default) or 26; 26 in cxx26
PROJECT_MARCH              # Value of -march; native in release, bench, and profile, empty elsewhere
PROJECT_LINKER             # auto (default), mold, lld, or default; auto picks mold, then lld, on Linux
CMAKE_CXX_COMPILER_LAUNCHER # ccache when found; set it empty to build without ccache
PROJECT_BUILD_APPS         # Build apps/; ON when top level
PROJECT_BUILD_TESTS        # Build tests/; ON when top level
PROJECT_BUILD_BENCHMARKS   # Build benchmarks/ and install the vcpkg benchmarks feature
PROJECT_BUILD_FUZZ         # Build tests/fuzz/; Clang only, ON in fuzz
PROJECT_FUZZ_SECONDS       # Seconds run_fuzz spends on each harness; default 30
PROJECT_FRAME_POINTERS     # -fno-omit-frame-pointer, for profilers; ON in bench
PROJECT_INSTALL            # Install and package rules; ON when top level
PROJECT_SANITIZER          # address, undefined, thread, memory, leak, or a list such as address;undefined
PROJECT_TRIPLET_VARIANT    # Builds dependencies with triplets/<host>-<variant>.cmake, for example asan
PROJECT_WARNINGS_AS_ERRORS # -Werror; ON when top level, never applied to consumers
PROJECT_STDLIB_HARDENING   # libstdc++ assertions and libc++ debug hardening; ON in dev
ENABLE_COVERAGE            # Instrument every target and add the coverage target; ON in coverage
```

Warnings and options come from the `myproj_warnings` and `myproj_options` interface targets in [cmake/CompilerPolicy.cmake](cmake/CompilerPolicy.cmake). The module and app helpers link both `PRIVATE`.

`release` binaries run only on CPUs with this machine's instruction set.
When the binary will run on other machines, build for a baseline CPU instead:

```bash
cmake --preset release -DPROJECT_MARCH=x86-64-v3
```

When the linker `auto` found fails on your code, link with the compiler's default:

```bash
cmake --preset dev -DPROJECT_LINKER=default
```

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

Every test executable links `myproj_test_support` from `tests/support/` instead of `gtest_main`.
It holds data builders, `FakeClock` for the `Clock` concept, and `AllocationGuard`.
Write plain `TEST`s named as CamelCase sentences; `tests/unit/core` and `tests/unit/parser` show each style once.

Fail a test when a hot path allocates; the guard reports the count at its own line:

```cpp
const myproj::test_support::AllocationGuard guard;
```

`ctest --preset dev` runs the `unit` and `integration` labels.
`tests/integration` uses the real modules and mocks only `core::OutputSink`, the edge the user sees.
`tests/functional` spawns `myproj_cli` and checks its exit code and output.
A GoogleTest suite whose name ends in `Stress` is labeled `stress` and runs only under the `stress` preset.
Only the suite name counts; a test name that ends in `Stress` stays in the default run.

Rerun the unit and integration tests of the `dev` build without rebuilding:

```bash
make test
```

Run the unit tests of one module:

```bash
ctest --preset dev -L core
```

Check the built apps as a user would run them, after changing an app or its output:

```bash
make functional
```

Run the long-running tests before a release or after touching a hot path:

```bash
make stress
```

Look for inputs that crash or break a promise of the parser, after changing code that reads input:

```bash
make fuzz
```

`tests/fuzz/<name>.cpp` is one harness, and `make dev` never builds it. [docs/how-to/fuzz.md](docs/how-to/fuzz.md) covers adding one and replaying a crash.

Look for memory errors, undefined behavior, and data races, after changing ownership or threading:

```bash
make asan tsan
```

Run the stress tests under AddressSanitizer after `make asan`, as CI does; `tsan-stress` is the same for ThreadSanitizer:

```bash
ctest --preset asan-stress
```

Dependencies are rebuilt with the same sanitizer. [docs/how-to/sanitizers.md](docs/how-to/sanitizers.md) covers skipping a test and suppressing a report.

On Linux, `make msan` looks for reads of uninitialized memory after a one-time libc++ build; [docs/how-to/msan.md](docs/how-to/msan.md) covers it.

Add a test kind directory, such as `tests/integration`, with one line; its `.cpp` files become `<kind>_tests`, labeled `<kind>`:

```cmake
project_add_tests(KIND integration DEPS myproj::core)
```

After changing `cmake/`, presets, or install rules, check the template's lifecycle cases:

```bash
scripts/selftest.sh
```

Each case prints `ok <case>`, or `skip <case>: <reason>` when this machine cannot run it; `scripts/selftest.sh --help` lists the cases.

## Lint

Check the tree with clang-tidy before pushing:

```bash
make lint
```

clangd shows the fast checks of `.clang-tidy` while you type. [CONTRIBUTING.md](CONTRIBUTING.md) shows how to suppress a finding.

## Format

The pre-commit hook formats staged C++ and CMake files and checks scripts and Markdown on every commit.

Run every hook over every file, as CI does, before pushing a large change:

```bash
pre-commit run --all-files
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

Configure the consumer with this project's compiler file and its dependencies, here `fmt`; replace `<myproj>` with this checkout and `<triplet>` with the directory under `vcpkg_installed/`:

```bash
cmake -S . -B build -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=<myproj>/cmake/toolchain/compiler.cmake \
  -DCMAKE_PREFIX_PATH="$HOME/.local;<myproj>/build/release/vcpkg_installed/<triplet>"
```

Build a release archive, written to `build/release/package/MyProj-<version>-<system>.tar.gz`:

```bash
cpack --config build/release/CPackConfig.cmake
```

The version lives only in `project(VERSION)` in [CMakeLists.txt](CMakeLists.txt). `#include <myproj/version.hpp>` from `myproj::core` gives `version_string`, `git_commit`, and `git_dirty`, captured at configure time. `myproj::core::version_banner()` from `<core/build_info.hpp>` formats all three.

When a module links a new package `PUBLIC`, add its `find_dependency` to [cmake/templates/Config.cmake.in](cmake/templates/Config.cmake.in).

## Benchmarks

`benchmarks/<module>/` holds `project_add_benchmark(MODULE <module>)` and becomes `<module>_benchmarks`.
`benchmarks/support/` provides the shared main, `flush_cache`, and `add_percentiles`; `benchmarks/parser` uses both.

Run every benchmark with ten repetitions and keep the JSON, after changing a hot path:

```bash
make bench
```

Check that a change did not slow anything down, against `main` or any other ref:

```bash
make bench-compare BASE=main
```

[docs/how-to/benchmark.md](docs/how-to/benchmark.md) covers machine preparation and hardware counters.

## VS Code Workflow

Open the folder, install the recommended extensions, pick a configure preset in the CMake Tools status bar, and press F5 to debug the launch target.
clangd, the Testing view, and Coverage Gutters read `build/current`, so they follow that preset.
[docs/how-to/debug-a-test.md](docs/how-to/debug-a-test.md) covers one test and a filter, and [docs/how-to/view-coverage.md](docs/how-to/view-coverage.md) covers coverage in the gutter.
[docs/how-to/dev-container.md](docs/how-to/dev-container.md) opens the same folder in a Linux container.

## CI

`.github/workflows/ci.yaml` runs on pull requests, the merge queue, and pushes to `main`.
Tier one runs the pre-commit hooks, `dev`, and `make lint` on Linux Clang. The other jobs start only when it passes.
`make setup-repo` requires only the `status` check; it fails when any job fails or is skipped when it should have run.

Draft pull requests run tier one only, and macOS runs `dev` on ready pull requests only.
A change to Markdown, `docs/`, or `LICENSE` alone skips every C++ step.

| Label on a pull request | Adds |
| --- | --- |
| `ci:full` | Every tier-two job, on a draft |
| `ci:msan` | The `msan` preset |
| `ci:bench` | `make bench-compare` against the base branch |

Codecov receives the coverage report when the repository secret `CODECOV_TOKEN` exists; it never fails a check.

`nightly.yaml` runs `msan`, `cxx26`, and `make valgrind` each night unless the last successful nightly checked the same commit, and on demand.
When you want real benchmark numbers each night, register a quiet self-hosted runner under a label and name it:

```bash
gh variable set BENCH_RUNNER_LABEL --body <label>
```

`image.yaml` rebuilds the CI image when a file `scripts/ci-image.sh` hashes changes, and on `main` publishes it after `make dev` passes inside it.
Until then CI runs `scripts/bootstrap.sh` instead, so a pull request never runs in the image its own change describes.
The package must be public, because CI looks for it without logging in.
A `v*` tag makes a release: see [docs/how-to/release.md](docs/how-to/release.md).

## Platform Notes

macOS is supported for everyday development: configure, build, tests, app runs, coverage, and basic benchmark smoke checks. Do not treat macOS benchmark CPU metadata or affinity behavior as authoritative for final latency claims.

Linux is the recommended platform for performance-sensitive work. For HFT-style benchmarking, use pinned compilers, `Release` or `RelWithDebInfo`, CPU governor/perf permissions, isolated cores, and preferably self-hosted runners or dedicated hardware.

## Troubleshooting

If clangd shows old commands, configure the preset you are working in again, here `dev`:

```bash
cmake --preset dev
```

If configure says vcpkg was not found, install it:

```bash
scripts/bootstrap.sh
```

The first configure of each preset builds dependencies and needs network access. vcpkg caches the binaries in `~/.cache/vcpkg/archives`, so later presets reuse them.

## Documentation

| Page | Contents |
| --- | --- |
| [Getting started](docs/getting-started.md) | First build to first breakpoint |
| [How-to guides](docs/how-to/README.md) | One page per task |
| [Reference](docs/reference/README.md) | Presets, helper API, test labels, conventions |
| [Decisions](docs/decisions.md) | Why each choice was made and what was rejected |
