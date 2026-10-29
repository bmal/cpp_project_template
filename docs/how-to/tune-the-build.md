# Tune the build

Each change below is one cache option on the configure command, which a [personal preset](personal-presets.md) can keep. The [options reference](../reference/options.md) lists all of them.

When a `release` binary will run on other x86-64 machines, build it and its dependencies for a baseline CPU:

```bash
cmake --preset release -DPROJECT_MARCH=x86-64-v3
```

When it will run on other arm64 machines, Apple Silicon or Graviton2 and later:

```bash
cmake --preset release -DPROJECT_MARCH=armv8.2-a
```

When a compiler upgrade raises a warning you cannot fix yet, build without `-Werror`:

```bash
cmake --preset dev -DPROJECT_WARNINGS_AS_ERRORS=OFF
```

When the linker `auto` picked fails on your code, link with the compiler's default:

```bash
cmake --preset dev -DPROJECT_LINKER=default
```

When you want to build without ccache:

```bash
cmake --preset dev -DCMAKE_CXX_COMPILER_LAUNCHER=
```

When you want to see which configurations exist on this machine:

```bash
cmake --list-presets=all
```
