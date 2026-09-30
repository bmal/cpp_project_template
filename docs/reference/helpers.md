# Helper API

`cmake/ProjectModules.cmake` defines one call per directory. `libs/` and `apps/` add every subdirectory that has a `CMakeLists.txt`.
Every compiled target links `myproj_warnings` and `myproj_options` privately, so nothing reaches a consumer.

## project_add_module

`project_add_module(NAME <n> [PUBLIC_DEPS ...] [PRIVATE_DEPS ...] [SOURCES ...] [NO_EXCEPTIONS] [NO_RTTI] [INTERNAL])`
creates `myproj_<n>`, alias `myproj::<n>`, with public include directory `libs/<n>/include`.
No `.cpp` under `src/` makes an `INTERFACE` library. When `tests/unit/<n>/` exists, its `.cpp` files become `<n>_unit_tests`.

| Option | Example | Effect |
| --- | --- | --- |
| `NAME` | `project_add_module(NAME net)` | Lower snake_case; target `myproj_net`, installed as `MyProj::net` |
| `PUBLIC_DEPS` | `project_add_module(NAME net PUBLIC_DEPS myproj::core)` | Linked `PUBLIC`; for types in public headers |
| `PRIVATE_DEPS` | `project_add_module(NAME net PRIVATE_DEPS fmt::fmt)` | Linked `PRIVATE`; needs sources under `src/` |
| `SOURCES` | `project_add_module(NAME net SOURCES src/socket.cpp)` | Replaces the default recursive glob of `src/*.cpp` |
| `NO_EXCEPTIONS` | `project_add_module(NAME book NO_EXCEPTIONS)` | Compiles the module's sources with `-fno-exceptions` |
| `NO_RTTI` | `project_add_module(NAME book NO_RTTI)` | Compiles the module's sources with `-fno-rtti` |
| `INTERNAL` | `project_add_module(NAME testing_hooks INTERNAL)` | Left out of the installed package |

## project_add_app

`project_add_app(NAME <n> [DEPS ...] [SOURCES ...])` creates executable `build/<preset>/bin/<n>` from the `.cpp` files of its directory.

| Option | Example | Effect |
| --- | --- | --- |
| `NAME` | `project_add_app(NAME myproj_cli)` | Lower snake_case executable name |
| `DEPS` | `project_add_app(NAME myproj_cli DEPS myproj::core)` | Linked `PRIVATE` |
| `SOURCES` | `project_add_app(NAME myproj_cli SOURCES main.cpp)` | Replaces the default recursive glob |

## project_add_tests

`project_add_tests(KIND <kind> [DEPS ...])` creates `<kind>_tests` from the `.cpp` files under the current directory, labeled `<kind>`.

| Option | Example | Effect |
| --- | --- | --- |
| `KIND` | `project_add_tests(KIND integration)` | Lower case; names the executable and the label |
| `DEPS` | `project_add_tests(KIND integration DEPS myproj::core)` | Linked with the test support library |

## project_add_benchmark

`project_add_benchmark(MODULE <n> [DEPS ...] [SOURCES ...])` creates `<n>_benchmarks` in `benchmarks/<n>/`, linked to `myproj::<n>`.

| Option | Example | Effect |
| --- | --- | --- |
| `MODULE` | `project_add_benchmark(MODULE parser)` | The module measured; it must exist under `libs/` |
| `DEPS` | `project_add_benchmark(MODULE parser DEPS myproj::core)` | Linked with the benchmark support library |
| `SOURCES` | `project_add_benchmark(MODULE parser SOURCES bench_parse.cpp)` | Replaces the default recursive glob |

## project_add_fuzz_target

`project_add_fuzz_target(NAME <n> [DEPS ...] [SOURCES ...])` creates the libFuzzer executable `<n>` from `<n>.cpp` in `tests/fuzz/`, seeded by `corpus/<n>/`.

| Option | Example | Effect |
| --- | --- | --- |
| `NAME` | `project_add_fuzz_target(NAME parser_fuzz)` | Lower snake_case; the harness file and corpus directory |
| `DEPS` | `project_add_fuzz_target(NAME parser_fuzz DEPS myproj::parser)` | Linked `PRIVATE` |
| `SOURCES` | `project_add_fuzz_target(NAME parser_fuzz SOURCES parser_fuzz.cpp)` | Replaces `<n>.cpp` |
