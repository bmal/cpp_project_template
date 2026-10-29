# C++ project template

A C++23 project on CMake presets and vcpkg, for Linux and macOS. It carries a weekend program to a multi-year, latency-sensitive system without changing tools.

- A module is a directory under `libs/`, an app a directory under `apps/`; each takes one CMake call.
- Unit, integration, functional, fuzz, and benchmark tests, with sanitizer and coverage presets.
- One strict warning set, clang-tidy, and formatters, the same in the editor, the hook, and CI.
- VS Code debugs the active preset with F5; CI runs the same preset names behind one `status` check.

## Quick start

When you start a project, create it from the template, clone it, and watch Init name it:

```bash
gh repo create order_book --public --clone --template bmal/cpp_project_template && cd order_book && until id=$(gh run list --json databaseId,workflowName -q 'map(select(.workflowName == "Init"))[0].databaseId') && [ -n "$id" ]; do sleep 2; done && gh run watch "$id"
```

When Init is done, which takes about a minute, fetch the rename, install the toolchain, and build and test:

```bash
git pull && scripts/bootstrap.sh && make dev
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
