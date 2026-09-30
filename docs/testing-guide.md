# Testing guide

A good test catches a regression, survives a refactoring, runs fast, and is easy to read.
The strategy follows Vladimir Khorikov's *Unit Testing: Principles, Practices, and Patterns*, with GoogleTest.

## Follow these rules

| Rule | In practice |
| --- | --- |
| A unit is a behavior, not a class | Test through the public interface; several real classes may take part |
| Prefer output-based tests | Call a function, check what it returns; then state-based, then mocks |
| Double only unmanaged edges | Mock what the user or another system sees, such as a sink or a socket; never an in-process neighbor |
| Inject time and other edges through a concept | Pass a `FakeClock` where production passes the system clock |
| Plain `TEST` over fixtures | Arrange in the test body or with a builder, so each test reads alone |
| Names are sentences | `Counter.SubtractBelowZeroFailsAndKeepsTheTotal`, in `CamelCase` |
| Coverage informs, never gates | Look at uncovered lines; do not write a test to raise a number |

## Pick the kind of test

| Kind | Where | When you want it |
| --- | --- | --- |
| Unit | `tests/unit/<module>/` | Every behavior of one module, including edge cases and errors |
| Integration | `tests/integration/` | Several modules wired together in one process, with a mock at the edge only |
| Functional | `tests/functional/` | A built app run as a user runs it: input in, exit code and output out |
| Fuzz | `tests/fuzz/` | Code that reads bytes it does not control |
| Benchmark | `benchmarks/<module>/` | Code whose speed is a requirement |
| Stress | A suite named `*Stress`, in any kind | A test that takes seconds; it runs only under the stress presets |

## Copy a sample style

| Style | Sample |
| --- | --- |
| Output-based | `tests/unit/parser/test_parser.cpp`, `tests/unit/core/test_build_info.cpp` |
| State-based, with a builder and the allocation guard | `tests/unit/core/test_counter.cpp` |
| A fake behind a concept seam | `tests/unit/core/test_stopwatch.cpp` |
| Parameterized over every malformed input | `ParseLineRejects` in `tests/unit/parser/test_parser.cpp` |
| A mock at the one unmanaged edge | `tests/integration/test_echo_fields.cpp` |
| Black box over an app | `tests/functional/test_cli.cpp` |

## Use the support library

`tests/support/` holds data builders, `FakeClock`, `AllocationGuard`, and `MYPROJ_SKIP_UNDER_SANITIZER`. It holds no logic.
When a hot path must never allocate, hold a guard across it; the test fails with the count at the guard's line:

```cpp
const myproj::test_support::AllocationGuard guard;
```

[Add tests](how-to/add-tests.md) says where each file goes; [test labels](reference/test-labels.md) says which preset runs it.
