# Use CI labels and the nightly

Every pull request runs tier one: the hooks, `dev`, and `make lint` on Linux Clang.
Tier two, macOS included, runs when tier one passes on a pull request that is not a draft.
The one required check is `status`. A change to Markdown, `docs/`, or `LICENSE` alone skips every C++ step.

When you want tier two on a draft, add a label:

```bash
gh pr edit --add-label ci:full
```

When you want MemorySanitizer on the pull request:

```bash
gh pr edit --add-label ci:msan
```

When you want the benchmarks compared against the base branch:

```bash
gh pr edit --add-label ci:bench
```

When you want the nightly now, with `msan`, `cxx26`, and `make valgrind`:

```bash
gh workflow run nightly.yaml
```

When you want real benchmark numbers each night, register a quiet self-hosted runner under a label and name it:

```bash
gh variable set BENCH_RUNNER_LABEL --body <label>
```

When the repository secret `CODECOV_TOKEN` exists, Codecov receives the coverage report; it never fails a check.
`image.yaml` publishes the CI image after `make dev` passes in it; the package must be public.
