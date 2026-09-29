# Find uninitialized reads with MemorySanitizer

MemorySanitizer runs on Linux with Clang only, and every C++ library in the build must be instrumented, the standard library included.

When you first use MSan on a machine, or after a Clang upgrade, build the instrumented libc++ once:

```bash
scripts/build-msan-libcxx.sh
```

It takes a few minutes and prints its directory under `~/.cache/myproj/msan-libcxx/`; a second run returns at once.

When you want to check a change for reads of memory that was never written:

```bash
make msan
```

A report names the read and, under `Uninitialized value was created by`, the allocation.
