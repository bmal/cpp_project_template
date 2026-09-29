# Fuzz code that reads input

When you want the fuzzer to search for crashing inputs, thirty seconds per harness:

```bash
make fuzz
```

You should see `Done <n> runs` and no `ERROR` for each harness. Search longer with `make fuzz FUZZ_SECONDS=300`.
New inputs collect in `build/fuzz/fuzz/<harness>/corpus/`, so the next run starts where this one stopped.

## Replay a crash

A crash stops the run and writes the input to `build/fuzz/fuzz/<harness>/crash-<hash>`.
When you want to debug it, run the harness on that one file:

```bash
build/fuzz/bin/parser_fuzz build/fuzz/fuzz/parser_fuzz/crash-<hash>
```

After the fix, copy the file into `tests/fuzz/corpus/<harness>/` under a name that says what it is.
`cmake --workflow --preset fuzz` replays every seed, so the crash cannot come back unnoticed.

## Add a harness

When a module parses bytes it does not control, write `tests/fuzz/<name>.cpp` with one function:

```cpp
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size);
```

Call the code with those bytes, call `std::abort()` when a result breaks a promise, and return 0.
Put at least one valid input in `tests/fuzz/corpus/<name>/`, then add one line to `tests/fuzz/CMakeLists.txt`:

```cmake
project_add_fuzz_target(NAME <name> DEPS myproj::<module>)
```

Fuzzing needs Clang; there is no `fuzz-gcc` preset.
