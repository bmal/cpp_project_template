# Cache options

Set on the configure command, as in `cmake --preset dev -DPROJECT_LINKER=default`, or in a personal preset.

| Option | Values | Default |
| --- | --- | --- |
| `PROJECT_COMPILER` | `clang` or `gcc` | `clang`; `gcc` in the `-gcc` presets |
| `PROJECT_CXX_STANDARD` | `23` or `26` | `23`; `26` in `cxx26` |
| `PROJECT_MARCH` | A `-march` value, such as `native` or `x86-64-v3` | Empty; `native` in `release`, `bench`, and `profile` |
| `PROJECT_LINKER` | `auto`, `mold`, `lld`, or `default` | `auto`: mold, then lld, on Linux, if it links |
| `CMAKE_CXX_COMPILER_LAUNCHER` | A launcher, or empty for none | `ccache` when found |
| `PROJECT_WARNINGS_AS_ERRORS` | `ON` or `OFF` | `ON` when top level; never applied to consumers |
| `PROJECT_STDLIB_HARDENING` | `ON` or `OFF` | `ON` in `dev` |
| `PROJECT_FRAME_POINTERS` | `ON` or `OFF` | `ON` in `bench` and `profile` |
| `PROJECT_SANITIZER` | `address`, `undefined`, `thread`, `memory`, `leak`, or a list such as `address;undefined` | Empty; set by the sanitizer presets |
| `PROJECT_TRIPLET_VARIANT` | A suffix such as `asan`, picking `triplets/<host>-<variant>.cmake` | Empty; set by the sanitizer presets |
| `PROJECT_BUILD_APPS` | `ON` or `OFF` | `ON` when top level |
| `PROJECT_BUILD_TESTS` | `ON` or `OFF` | `ON` when top level |
| `PROJECT_BUILD_BENCHMARKS` | `ON` or `OFF`; `ON` installs the vcpkg `benchmarks` feature | `ON` in `bench` |
| `PROJECT_BUILD_FUZZ` | `ON` or `OFF`; Clang only | `ON` in `fuzz` |
| `PROJECT_FUZZ_SECONDS` | Seconds `run_fuzz` spends on each harness | `30` |
| `PROJECT_INSTALL` | `ON` or `OFF` | `ON` when top level |
| `ENABLE_COVERAGE` | `ON` or `OFF`; adds the `coverage` target | `ON` in the coverage presets |
| `VCPKG_ROOT` | Environment variable naming a vcpkg checkout | `.vcpkg/` from `scripts/bootstrap.sh` |

Warnings and options reach every internal target through the `myproj_warnings` and `myproj_options` interface targets in `cmake/CompilerPolicy.cmake`.
