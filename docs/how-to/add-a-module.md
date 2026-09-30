# Add a module or an app

A module is a directory under `libs/`, an app one under `apps/`. Neither touches a shared CMake file.

## Add a module

When you want a new library `myproj::net` with a namespace stub and one passing unit test:

```bash
scripts/new-module.sh net
```

You should see `libs/net/` and `tests/unit/net/`. The next `make dev` builds and runs `net_unit_tests`.
To link another module, edit the one call in `libs/net/CMakeLists.txt`:

```cmake
project_add_module(NAME net PUBLIC_DEPS myproj::core)
```

A module with no `.cpp` under `src/` is header-only. Every option is in the [helper API](../reference/helpers.md).

## Add an app

When you want a new executable `build/<preset>/bin/tool` that links `myproj::core`:

```bash
scripts/new-app.sh tool
```

To run it after a `dev` build:

```bash
build/dev/bin/tool
```

## Add a source file

Create the file under `src/` or the app directory; the next build picks it up without a configure.
