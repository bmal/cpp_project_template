# View coverage

Coverage shows which lines of `libs/` the tests run. It is never a gate.

## See coverage in the editor

When you want covered and uncovered lines marked in VS Code, run the tests under the coverage preset:

```bash
make coverage
```

Then run **Coverage Gutters: Display Coverage**. It reads `build/current/coverage/lcov.info`, so configure another preset only after you are done looking.

## Check GCC's view on Linux

When you want coverage as GCC's build sees it, run the GCC variant:

```bash
cmake --preset coverage-gcc && cmake --build --preset coverage-gcc --target coverage
```
