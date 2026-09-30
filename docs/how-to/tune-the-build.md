# Tune the build

Each change below is one cache option on the configure command. The [options reference](../reference/options.md) lists all of them.

When a `release` binary will run on other machines, build for a baseline CPU instead of this one:

```bash
cmake --preset release -DPROJECT_MARCH=x86-64-v3
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

To keep one of these settings, put it in a [personal preset](personal-presets.md).
