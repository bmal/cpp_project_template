# Test labels

Every test gets its labels from where it lives; no test names a label itself.

| Label | Tests | Comes from |
| --- | --- | --- |
| `unit` | Every test under `tests/unit/` | The module helper |
| `<module>` | The unit tests of one module, such as `core` | The module helper |
| `integration` | `tests/integration/` | `project_add_tests(KIND integration)` |
| `functional` | `tests/functional/`, which spawn the built apps | `project_add_tests(KIND functional)` |
| `stress` | Tests whose GoogleTest suite name ends in `Stress`, in any directory | The suite name; test and parameter names never count |
| `fuzz` | One replay of each harness's seed corpus | `project_add_fuzz_target` |
| `bench` | One run of each benchmark executable | `project_add_benchmark` |

A `stress` test keeps its other labels, but only the stress presets run it.

| Test preset | Runs |
| --- | --- |
| `dev`, `cxx26`, and their `-gcc` twins | `unit` and `integration` |
| `functional` | `functional`, on the `dev` build |
| `stress` | `stress`, on the `dev` build |
| `release`, `relwithdebinfo`, `profile`, `coverage`, and their `-gcc` twins | Everything built except `stress` |
| `asan`, `tsan`, `msan`, and the `asan-gcc` and `tsan-gcc` twins | Everything built except `stress`; these presets build no apps |
| `asan-stress`, `tsan-stress` | `stress` under the sanitizer, each test capped at five minutes |
| `fuzz` | `fuzz` |
| `bench` | `bench` |

When you want one label of an existing build, add `-L` to its test preset:

```bash
ctest --preset dev -L core
```
