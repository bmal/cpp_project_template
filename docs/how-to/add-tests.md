# Add tests

The [testing guide](../testing-guide.md) says which kind of test to write. This page says where it goes.

## Test one module

When you want tests for module `net`, create its mirror directory and a file in it:

```bash
mkdir -p tests/unit/net && touch tests/unit/net/test_socket.cpp
```

Every `.cpp` there becomes `net_unit_tests`, labeled `unit` and `net`. To run only those:

```bash
ctest --preset dev -L net
```

A `tests/unit/<name>/` with no module `libs/<name>/` fails configure and names the directory.

## Test across modules or over an app

When a test wires several modules, add a file to `tests/integration/`; `make dev` runs it.
When a test runs a built app as a user would, add a file to `tests/functional/`, then run:

```bash
make functional
```

## Mark a long-running test

When a test takes seconds, end its suite name in `Stress`; `make dev` skips it and this runs it:

```bash
make stress
```
