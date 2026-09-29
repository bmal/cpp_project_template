# Decision register

Ratified by the owner on 2026-09-28 in issue #2. Implement to it; do not re-open it.
Each entry states the decision, why, and what was rejected.
Narrow choices left to single tickets are appended below the entry they refine.

## Foundation

### D1. CMake stays

- Decision: CMake, minimum 3.28, presets schema 8. The layout stays migration-friendly with one target per directory.
- Why: one tool carries a project from a weekend program to a multi-year system without a migration.
- Rejected: Bazel, as a replacement or in parallel (answers issue #1).

### D2. Module-per-directory layout

- Decision: `libs/<module>/{include/<module>,src}`, `apps/<name>`, `tests/<kind>`, `benchmarks/<module>`, `cmake/`, `scripts/`, `docs/`, `triplets/`.
- Why: a module is a self-contained unit you can copy, and apps are visibly separate from libraries.
- Rejected: the `standalone/` and `all/` directories, and one library target that globs everything.

### D3. Central test tree with a drift guard

- Decision: unit tests mirror modules under `tests/unit/<module>`. Configure fails on a test directory with no module.
- Why: the production tree is exactly what ships, and the mirror cannot drift silently.
- Rejected: tests inside each module directory, which would stop the production tree being exactly what ships.

### D4. Five test kinds

- Decision: unit, integration, functional over `apps/` binaries, fuzz, benchmark. Stress is a CTest label. Install smoke is a CI step.
- Why: each kind proves a different thing, and the default run stays unit plus integration.
- Rejected: a stress directory duplicating structure; the old two test kinds.

#### D4.1. The stress label comes from the suite name

- Decision: a GoogleTest suite whose name ends in `Stress` is labeled `stress`, typed and parameterized suites included. Test names and parameter names never count.
- Why: the label is visible where the test is written, and no CMake changes when a stress test is added.
- Rejected: a label per test in CMake, which drifts from the code; a `tests/stress/` directory, already rejected by D4.

### D5. Test granularity

- Decision: one executable per module for unit and benchmark; one for integration; one for functional; one per fuzz harness.
- Why: touching a test relinks one binary, and CTest runs modules in parallel.
- Rejected: none recorded.

### D6. vcpkg in manifest mode

- Decision: vcpkg is the shipped manager. Project CMake speaks only `find_package`. Triplets: default, `hft`, one per sanitizer needing instrumented dependencies.
- Why: dependencies are built by the same compiler and flags as the project, so TSan and HFT flags are exact.
- Rejected: CPM; shipping Conan or a second manager next to `vcpkg.json`. Conan is documented as the team-scale swap.

### D7. Linux and macOS only

- Decision: CI compilers are GCC and Clang on Linux, Homebrew LLVM on macOS. Configure enforces GCC 14 and Clang 19. AppleClang is best-effort and not in CI.
- Why: an old compiler fails in one plain sentence, not in a template instantiation error.
- Rejected: Windows and MSVC in any form; their conditionals are removed.

### D8. C++23 by default

- Decision: C++23 default. `PROJECT_CXX_STANDARD` accepts 26. A `cxx26` preset runs as a nightly canary.
- Why: the project is ready for C++26 when the standard library is.
- Rejected: none recorded.

## Compilation policy

### D9. Warnings and options interface targets

- Decision: `myproj_warnings` and `myproj_options`, linked privately by every internal target, tests included. `-Werror` is on when top level, with one option to disable, never for consumers.
- Why: the library is not the least-checked code, and a new compiler never blocks you.
- Rejected: propagating flags to consumers; exempting tests.

### D10. Own sanitizer module

- Decision: presets `asan` (ASan plus UBSan), `tsan`, `msan`, `fuzz`. MSan ships with a libc++ build script, opt-in, nightly. Runtime options live in test presets. `dev` hardens the standard library. Skip macros work on GCC.
- Why: each bug class has a button, and local and CI runs behave the same.
- Rejected: the third-party cmake-scripts dependency; manual `__SANITIZE_*` definitions in CMake.

#### D10.1. Fuzz ships before the sanitizer module

- Decision: the `fuzz` preset sets `PROJECT_SANITIZE` to `fuzzer-no-link,address,undefined`, the option `asan` and `tsan` already use. A harness adds `-fsanitize=fuzzer`. The sanitizer module moves all three presets to `PROJECT_SANITIZER` together.
- Why: milestone 2 is done when every test kind has a passing example, and fuzz is one of the five kinds.
- Rejected: rewording the milestone to four kinds; holding milestone 2 open until the sanitizer module lands.

#### D10.2. Fuzz harnesses also run as a CTest test

- Decision: each harness has a test labeled `fuzz` that replays its seed corpus once. `run_fuzz` does the searching, for `PROJECT_FUZZ_SECONDS` per harness, and writes new inputs and crash files under `build/fuzz/fuzz/<harness>/`.
- Why: a fixed crash stays fixed once its input is a seed, and fuzzing never changes the source tree.
- Rejected: fuzzing inside CTest, which makes the run time of a test preset unbounded.

#### D10.3. No function sanitizer in harness files on macOS

- Decision: on macOS a harness source is compiled with `-fno-sanitize=function`. Modules keep the check.
- Why: Apple `ld` fails with "invalid r_symbolnum" on some harness objects that combine the check with fuzzer coverage.
- Rejected: the deprecated `-ld_classic` linker; dropping UBSan from the preset.

### D11. clang-tidy is the only analyzer

- Decision: a `lint` target over `compile_commands.json`, also in CI. Alias checks disabled. `misc-include-cleaner` covers includes. clangd reads `.clang-tidy` with a fast filter.
- Why: one bug produces one report, and the editor and CI agree.
- Rejected: cppcheck; include-what-you-use; a duplicated check block in the clangd config. CodeChecker, CodeQL, and GCC `-fanalyzer` are documented as optional upgrades.

### D12. Formatters through pre-commit

- Decision: clang-format with every option pinned, gersemi, shellcheck, markdownlint, all through pre-commit. CI runs the same config.
- Why: formatting is never a review comment.
- Rejected: none recorded.

### D13. Naming and style

- Decision: snake_case functions, variables, namespaces, constants. CamelCase types and enum values. Trailing-underscore members. Prefixed UPPER_CASE macros. LLVM base, 4 spaces, 100 columns.
- Why: the convention is encoded in clang-tidy, so `lint` is clean on a fresh clone.
- Rejected: none recorded.

## Developer experience

### D14. Debug flags per preset

- Decision: debug flags per preset, `-fstandalone-debug` on Clang, frame pointers everywhere. `relwithdebinfo` and `profile` presets. Opt-in debugger init files. `rr` and sanitizer debugging documented.
- Why: the debugger shows full types, and `perf` output is readable.
- Rejected: auto-loaded debugger init files; split DWARF by default.

### D15. Presets as the single source of truth

- Decision: `dev`, `release`, `relwithdebinfo`, `profile`, `asan`, `tsan`, `msan`, `fuzz`, `coverage`, `bench`, `cxx26`, plus `-gcc` variants and workflow presets. Same names locally and in CI. `CMakeUserPresets.json` is gitignored.
- Why: you type the same name on every platform, and green means the same thing everywhere.
- Rejected: CI flags in YAML that differ from local presets.

### D16. One toolchain shim

- Decision: it locates vcpkg, picks the compiler from `PROJECT_COMPILER`, fixes the Homebrew libc++ link path, and chainloads vcpkg. Triplets chainload the same compiler file.
- Why: every environment assumption lives in one file, and a missing tool prints the fixing command.
- Rejected: none recorded.

### D17. `build/current`

- Decision: configure refreshes a `build/current` symlink and a root `compile_commands.json` link. Every tool points at them.
- Why: clangd, the test explorer, and coverage follow the last configured preset.
- Rejected: tools hard-wired to `build/dev`.

### D18. VS Code

- Decision: CMake Tools in presets mode, clangd, CodeLLDB, C++ TestMate, Coverage Gutters. `launch.json` uses the launch-target variable and a test-filter prompt. `.editorconfig`.
- Why: F5 hits a breakpoint with no configuration.
- Rejected: the CMake Tools test explorer; Microsoft IntelliSense; editors other than VS Code (CLion works through presets).

### D19. Fast rebuilds and `make help`

- Decision: ccache when found. mold, else lld, else default on Linux; the Apple linker on macOS. A root `Makefile` with `make help`, convenience only.
- Why: rebuilds are fast without configuration, and no one looks up a preset command.
- Rejected: Makefile targets that are not a preset or target.

## Extension and code

### D20. Module helper

- Decision: `project_add_module`, `project_add_app`, `project_add_benchmark`, `project_add_fuzz_target`. Knobs `NO_EXCEPTIONS`, `NO_RTTI`, `INTERNAL`, optional `SOURCES`. `CONFIGURE_DEPENDS` glob by default. `libs/` and `apps/` self-register. Unit tests attach when their directory exists. `gtest_discover_tests` in `PRE_TEST` mode. `new-module` and `new-app` scripts.
- Why: adding a module never touches shared build files, and adding a file is just adding a file.
- Rejected: none recorded.

#### D20.1. A helper for test kinds

- Decision: `project_add_tests(KIND <kind> [DEPS ...])` builds `<kind>_tests` from a directory under `tests/` and labels it `<kind>`. Integration and functional tests use it.
- Why: D5 asks for one executable per kind, and every test executable gets the same support library, warnings, and stress rule.
- Rejected: hand-written `add_executable` and `gtest_discover_tests` calls in each kind directory.

### D21. Testing strategy

- Decision: Khorikov's principles with GoogleTest. Classical school, doubles only at unmanaged edges, output-based preferred, plain `TEST` over fixtures, CamelCase sentence names. `tests/support/` holds builders, fake clock, allocation guard, skip macros, nothing with logic.
- Why: tests resist refactoring, and the strategy is visible in code.
- Rejected: coverage as a gate, including the old `codecov.yaml` thresholds; mocks of in-process neighbors.

### D22. Benchmarks

- Decision: the `bench` preset shares the release flag base. A support library with cache flush and percentiles. libpfm counters on Linux where the port allows. A machine-prep script. Smoke in CI, real numbers nightly on a self-hosted label. `make bench-compare BASE=main`. JSON to `build/current/bench/`.
- Why: you measure the program you ship, repeatably, with results that are never lost.
- Rejected: real numbers on hosted runners as a gate; Cachegrind and Callgrind in CI.

#### D22.1. Frame pointers and benchmark dependencies are opt-in

- Decision: `PROJECT_FRAME_POINTERS` adds `-fno-omit-frame-pointer` through `myproj_options`; `bench` turns it on. Google Benchmark is the vcpkg manifest feature `benchmarks`, installed only when `PROJECT_BUILD_BENCHMARKS` is on.
- Why: `bench` is `release` plus one flag, and no other preset pays for a dependency it never links.
- Rejected: a separate flag list for `bench`; Google Benchmark as a default dependency.

#### D22.2. The comparison tool runs from a local virtual environment

- Decision: `make bench-compare` creates `build/bench-venv/` on first use and installs `numpy` and `scipy` from PyPI, unpinned, for Google Benchmark's `compare.py`.
- Why: the tool needs both, and a virtual environment under `build/` touches nothing outside the repository.
- Rejected: installing them in `bootstrap.sh` for everyone; pinned versions that nothing would update.

### D23. Sample code, option C

- Decision: `core` compiled, `parser` header-only and exception-free, one CLI app, one example per test kind, a mock at an edge in integration tests. Two-line purpose header per file. Placeholders `myproj` and `MyProj`.
- Why: every sample file demonstrates one mechanism, so the samples are the documentation.
- Rejected: none recorded.

## Lifecycle

### D24. Install, version, packaging

- Decision: standard CMake install modules, one export set, `SameMajorVersion`. Version single-sourced in `project()`. A generated version header with the git hash captured at configure. CPack TGZ. Install smoke covers `find_package` and `FetchContent`.
- Why: consumers link `MyProj::<module>`, and every app knows which commit is running.
- Rejected: DEB, RPM, and other CPack generators.

#### D24.1. Install rules only when top level

- Decision: every install and package rule needs `PROJECT_INSTALL`, which is ON only when the project is top level.
- Why: a `FetchContent` consumer's install tree holds only its own files, and a consumer that re-exports a module can still opt in.
- Rejected: a top-level gate with no option; asking every consumer to pass `EXCLUDE_FROM_ALL`.

### D25. CI

- Decision: `ci.yaml` on `pull_request`, `merge_group`, and push to `main`. Tier one on Linux Clang, then fan-out. One required `status` check. macOS on non-draft PRs, `dev` only. Labels `ci:full`, `ci:bench`, `ci:msan`. `nightly.yaml` skips when `main` is unchanged. `release.yaml` on `v*` tags. Actions pinned by SHA with Dependabot. Least privilege, timeouts, seven-day artifacts, in-job path filters. A composite setup action. Linux jobs in the shared image with a bootstrap fallback.
- Why: a broken push costs one Linux job, and branch protection never changes with the matrix.
- Rejected: CI on every branch push; one required check per job; shipping CodeQL (documented instead).

### D26. Instantiation

- Decision: literal placeholders. Idempotent `init-project.sh <name> [--strip-samples]`. A self-initializing workflow on first push, also manual. `bootstrap.sh`. `.devcontainer/` on the CI Dockerfile. `setup-repo.sh` for labels, ruleset, merge queue, Dependabot. CI exercises the init script.
- Why: a named, protected, green repository in under ten minutes.
- Rejected: a templating engine such as Copier or cookiecutter; the rename cache variable.

### D27. Documentation per Diátaxis

- Decision: a one-screen README, a one-page getting started, how-tos under forty lines, generated reference, one testing guide, this register. Short, command first.
- Why: a stranger does the task and leaves.
- Rejected: documenting what a preset name or a `make help` line already says.

### D28. Delivery

- Decision: one program, one parent issue, one register, five milestones, an adversarial review per milestone, then one final review. Every child names acceptance commands at a seam and closes only after running them.
- Why: the template stays buildable and green at every commit.
- Rejected: none recorded.
