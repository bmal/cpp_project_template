# C++ project template

A C++23 project on CMake presets and vcpkg, for Linux and macOS. It carries a weekend program to a multi-year, latency-sensitive system without changing tools.

- A module is a directory under `libs/`, an app a directory under `apps/`; each takes one CMake call.
- Unit, integration, functional, fuzz, and benchmark tests, with sanitizer and coverage presets.
- One strict warning set, clang-tidy, and formatters, the same in the editor, the hook, and CI.
- VS Code debugs the active preset with F5; CI runs the same preset names behind one `status` check.

## Quick start

Create your repository from the template on GitHub, then clone it, install the toolchain, and build:

```bash
gh repo create order_book --public --clone --template bmal/cpp_project_template
cd order_book && scripts/bootstrap.sh
make dev
```

The Init workflow names the project after the repository within a minute.
When `gh run watch` finds no run in progress, fetch the rename:

```bash
git pull
```

When the repository is named, require the `status` check on `main` and allow only squash merges:

```bash
make setup-repo
```

When you want to see every other action:

```bash
make help
```

## Layout

| Path | Holds |
| --- | --- |
| `libs/<module>/` | A library: public headers under `include/<module>/`, sources under `src/` |
| `apps/<name>/` | An executable |
| `tests/<kind>/` | Unit tests under `unit/<module>/`, then integration, functional, and fuzz |
| `benchmarks/<module>/` | Google Benchmark executables |
| `cmake/`, `triplets/`, `scripts/` | Build helpers, toolchain file, vcpkg triplets, and bootstrap, init, and scaffolding |

## Documentation

| Page | Contents |
| --- | --- |
| [Getting started](docs/getting-started.md) | First ten minutes, from a new repository to a breakpoint |
| [How-to guides](docs/how-to/README.md) | One page per task |
| [Testing guide](docs/testing-guide.md) | What to test, at which level, and how |
| [Reference](docs/reference/README.md) | Commands, presets, options, helper API, test labels, conventions |
| [Decisions](docs/decisions.md) | Why each choice was made and what was rejected |
| [Contributing](CONTRIBUTING.md) | How the docs and the code are written |
