# Lint and format

clang-tidy is the only analyzer; clangd shows its fast checks while you type.
The pre-commit hook formats staged C++ and CMake files and checks scripts and Markdown.

When you want the findings CI would report, before you push:

```bash
make lint
```

When you changed code under an `#if` that only one preset compiles, lint that preset:

```bash
make lint PRESET=asan
```

When you want every file formatted in place:

```bash
make format
```

When you want every hook over every file, as CI runs them, before pushing a large change:

```bash
pre-commit run --all-files
```

When the code is right and a check is wrong for one line, name the check and the reason above it:

```cpp
// NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast): bytes in, characters out.
```

A bare `NOLINT` is not accepted. The [conventions](../reference/conventions.md) give the full rule.
