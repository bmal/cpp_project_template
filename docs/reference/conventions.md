# Conventions

`make lint` and `make format-check` enforce the naming and layout rules; the rest is checked in review.

## Layout

| Path | Holds |
| --- | --- |
| `libs/<module>/include/<module>/` | Public headers, included as `<module/header.hpp>` |
| `libs/<module>/src/` | Sources and private headers; none makes the module header-only |
| `apps/<name>/` | One executable |
| `tests/unit/<module>/` | Unit tests; configure fails when no module `<module>` exists |
| `tests/integration/`, `tests/functional/`, `tests/fuzz/` | One executable per kind, one per fuzz harness |
| `tests/support/` | Builders, `FakeClock`, `AllocationGuard`, skip macros, and the test main; no logic |
| `tests/sanitizers/` | Suppression files the sanitizer test presets read |
| `benchmarks/<module>/`, `benchmarks/support/` | One benchmark executable per module, and its shared main |
| `cmake/`, `triplets/`, `scripts/`, `tools/` | Build helpers, vcpkg triplets, scripts, and debugger init files |

## Naming

| Kind | Style | Example |
| --- | --- | --- |
| Namespaces, functions, variables, parameters, constants | `snake_case` | `echo_fields`, `max_depth` |
| Classes, structs, enums, enum values, aliases, concepts, template type parameters | `CamelCase` | `OutputSink`, `Clock` |
| Private and protected members | `snake_case_` | `count_` |
| Macros | `MYPROJ_UPPER_CASE` | `MYPROJ_SKIP_UNDER_SANITIZER` |
| Test suites and tests | `CamelCase` sentences; a suite ending in `Stress` is a stress test | `Counter.AddAccumulates` |
| Files | `snake_case.hpp` and `.cpp`; tests `test_<subject>.cpp` | `test_counter.cpp` |

Formatting is LLVM style with 4 spaces and 100 columns, pinned in `.clang-format`; `make format` applies it.

## Suppressing a clang-tidy finding

Fix the finding when you can. When the check is wrong for one line, name the check and give a one-sentence reason:

```cpp
// NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): bytes in, characters out.
const std::string_view line(reinterpret_cast<const char*>(data), size);
```

Wrap a block in `NOLINTBEGIN(check-name): reason` and `NOLINTEND(check-name): reason`.
A bare `NOLINT`, without a check name or a reason, is not accepted.
To turn a check off everywhere, change `.clang-tidy` and record why in [decisions.md](../decisions.md).

## File headers

Every source, header, and CMake file under `libs/`, `apps/`, `tests/`, and `benchmarks/` opens with two comment lines.
The first names the mechanism the file demonstrates, the second what it does. `scripts/selftest.sh sample_headers` checks it.
