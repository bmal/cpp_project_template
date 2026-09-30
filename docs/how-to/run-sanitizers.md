# Run sanitizers

When you changed ownership, lifetimes, or threading, run every test under ASan with UBSan, then TSan; a report fails the test:

```bash
make asan tsan
```

When you want the tests whose suite ends in `Stress` under ASan too, as CI runs them, after `make asan`:

```bash
ctest --preset asan-stress
```

When you want the report of one failing test without the rest of the suite:

```bash
ctest --preset asan -R '^Counter\.' --output-on-failure
```

When a test cannot work under one sanitizer, such as a death test under TSan, skip it with the reason:

```cpp
MYPROJ_SKIP_UNDER_SANITIZER(TSAN, "a death test forks a process that already runs threads");
```

It comes from `support/sanitizers.hpp`; the names are `ASAN`, `TSAN`, and `MSAN`, on GCC and Clang. `ctest` lists the test as skipped.

When a report comes from a dependency, add one line with the reason above it to `tests/sanitizers/<tool>.supp`:

```text
race:third_party::Queue::push
```

UBSan stops at the first report and reads no file; mark the one function instead:

```cpp
__attribute__((no_sanitize("alignment"))) std::uint32_t read_unaligned(const std::byte* data);
```
