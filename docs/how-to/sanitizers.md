# Skip a test or suppress a report under a sanitizer

`make asan` and `make tsan` run every test under a sanitizer; a report fails the test and says where.

## Rerun one test

When you want the report of one failing test without the rest of the suite:

```bash
ctest --preset asan -R '^Counter\.' --output-on-failure
```

## Skip a test one sanitizer cannot run

When a test cannot work under one sanitizer, such as a death test under TSan, skip it with the reason:

```cpp
#include "support/sanitizers.hpp"

MYPROJ_SKIP_UNDER_SANITIZER(TSAN, "a death test forks a process that already runs threads");
```

The names are `ASAN`, `TSAN`, and `MSAN`, on GCC and Clang. `ctest` lists the test as skipped.

## Suppress a report in code you do not own

When a report comes from a dependency, add one line with the reason above it to `tests/sanitizers/<tool>.supp`:

```text
race:third_party::Queue::push
```

UBSan stops at the first report and reads no file; mark the one function instead:

```cpp
__attribute__((no_sanitize("alignment"))) std::uint32_t read_unaligned(const std::byte* data);
```
