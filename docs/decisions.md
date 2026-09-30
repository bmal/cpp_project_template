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

#### D10.4. Fuzzer coverage comes from PROJECT_BUILD_FUZZ

- Decision: `PROJECT_SANITIZER` names sanitizers only; the `fuzz` preset sets it to `address;undefined`, and `PROJECT_BUILD_FUZZ` adds `fuzzer-no-link` to `myproj_options`.
- Why: every value of `PROJECT_SANITIZER` is a sanitizer, and fuzzing without a sanitizer still gets coverage.
- Rejected: `fuzzer-no-link` as a `PROJECT_SANITIZER` value.

#### D10.5. Sanitizer presets pick their triplet by variant

- Decision: `asan` and `tsan` set `PROJECT_TRIPLET_VARIANT`, and the toolchain selects `triplets/<arch>-<os>-<variant>.cmake`. There is no UBSan suppression file.
- Why: a preset cannot name the host architecture, and UBSan ignores suppressions under `-fno-sanitize-recover=all`.
- Rejected: one preset per architecture; a `ubsan.supp` that would never be read.

#### D10.6. The MSan libc++ follows the project's Clang

- Decision: `scripts/build-msan-libcxx.sh` builds libc++ for the Clang the toolchain picks, into `~/.cache/myproj/msan-libcxx/<full Clang version>/`. The `msan` preset exists on every host and fails configure in one sentence on macOS, on GCC, or when that build is missing. The selftest case reports itself skipped on macOS and on Linux without the build.
- Why: libc++ headers must match the compiler that includes them, and a hidden preset would fail with CMake's generic "disabled preset" message.
- Rejected: a directory per Clang major, which a point upgrade would silently reuse; a preset condition that hides `msan` on macOS.

#### D10.7. Sanitizer presets need a symbolizer

- Decision: `bootstrap.sh` installs `llvm-<major>` on Linux, which holds `llvm-symbolizer`. A Clang sanitizer or fuzz configure on Linux fails in one sentence without it. The selftest plants pass only when a frame names the planted test.
- Why: without a symbolizer a report has no function names, and a suppression by name never matches.
- Rejected: a symbolizer path in the test presets, which cannot name a path for every host; a warning, which scrolls past.

#### D10.8. Leaks are reported on every host

- Decision: the `asan` and `fuzz` test presets set `detect_leaks=1`. `lsan.supp` suppresses the one leak in libFuzzer's own driver on macOS.
- Why: macOS leaves leak detection off by default, so a leak passed locally and failed on Linux.
- Rejected: leaving the default, which makes one preset name mean two things.

### D11. clang-tidy is the only analyzer

- Decision: a `lint` target over `compile_commands.json`, also in CI. Alias checks disabled. `misc-include-cleaner` covers includes. clangd reads `.clang-tidy` with a fast filter.
- Why: one bug produces one report, and the editor and CI agree.
- Rejected: cppcheck; include-what-you-use; a duplicated check block in the clangd config. CodeChecker, CodeQL, and GCC `-fanalyzer` are documented as optional upgrades.

#### D11.1. Checks disabled beyond the aliases

- Decision: off are `modernize-use-trailing-return-type`, `readability-magic-numbers`, `readability-identifier-length`, `portability-avoid-pragma-once`, `cppcoreguidelines-owning-memory`, `cppcoreguidelines-pro-bounds-array-to-pointer-decay`, and `cppcoreguidelines-pro-bounds-avoid-unchecked-container-access`. Cognitive complexity fails above 25 and ignores macros. `cppcoreguidelines-macro-usage` allows `MYPROJ_` macros. Pointer arithmetic, `reinterpret_cast`, and swappable parameters stay on.
- Why: each disabled check fights D13, a C API boundary, idiomatic short names, or every `operator[]`, and stdlib hardening plus ASan already catch out-of-bounds indexing at run time. The checks that stay on mark wire parsing and price-for-quantity swaps, which deserve a reason in review.
- Rejected: the ticket's list alone, which needs a `NOLINT` on every index and every header; magic numbers on in `libs/` only, which needs a second config file.

#### D11.2. A suppression names its check and gives a reason

- Decision: `// NOLINTNEXTLINE(check-name): reason in one sentence.` `NOLINTBEGIN` and `NOLINTEND` take the same form; a bare `NOLINT` is not allowed.
- Why: a reviewer can judge the suppression without opening the check's documentation.
- Rejected: a trailing `// NOLINT(check-name)` as the only form.

#### D11.3. No C++20 module scanning

- Decision: `CMAKE_CXX_SCAN_FOR_MODULES` is off, and `make lint` configures its preset before running.
- Why: scanning puts `.modmap` files that exist only after a build into `compile_commands.json`, so clang-tidy and clangd fail on a fresh clone.
- Rejected: making `lint` build everything first, which reports the compiler's findings before its own.

#### D11.4. Lint is clean on every preset

- Decision: `make lint PRESET=<preset>` is clean for every configure preset, and the selftest case `lint_presets` checks each one.
- Why: code under `#if` is compiled by some presets only, and `dev` alone never reads it.
- Rejected: linting `dev` only; excluding the conditional code from lint.

### D12. Formatters through pre-commit

- Decision: clang-format with every option pinned, gersemi, shellcheck, markdownlint, all through pre-commit. CI runs the same config.
- Why: formatting is never a review comment.
- Rejected: none recorded.

#### D12.1. clang-format and gersemi run from the bootstrap-pinned install

- Decision: `scripts/format.sh` runs clang-format and gersemi for `make format`, `make format-check`, and two local pre-commit hooks. Bootstrap pins gersemi and pre-commit through pipx; clang-format is the pinned LLVM major. Other hooks come from pinned hook repositories. Fuzz seeds are excluded, and Markdown line length is not checked.
- Why: the check and the hook share one binary, so they never disagree. `.clang-format` is the dump of one clang-format major.
- Rejected: the upstream clang-format and gersemi hooks, a second install `make format-check` would not use. A check through pre-commit, which rewrites files while checking.

### D13. Naming and style

- Decision: snake_case functions, variables, namespaces, constants. CamelCase types and enum values. Trailing-underscore members. Prefixed UPPER_CASE macros. LLVM base, 4 spaces, 100 columns.
- Why: the convention is encoded in clang-tidy, so `lint` is clean on a fresh clone.
- Rejected: none recorded.

#### D13.1. The style choices LLVM leaves open

- Decision: on the LLVM base, access modifiers sit flush with `class`, and `template <...>` always gets its own line.
- Why: this is how the code was written before a formatter checked it.
- Rejected: merging `template <...>` onto the declaration, which LLVM does for short ones.

## Developer experience

### D14. Debug flags per preset

- Decision: debug flags per preset, `-fstandalone-debug` on Clang, frame pointers everywhere. `relwithdebinfo` and `profile` presets. Opt-in debugger init files. `rr` and sanitizer debugging documented.
- Why: the debugger shows full types, and `perf` output is readable.
- Rejected: auto-loaded debugger init files; split DWARF by default.

#### D14.1. lldb is the terminal debugger bootstrap installs, with its symbol server off

- Decision: Linux bootstrap installs `lldb-<version>` as plain `lldb`, and no gdb. `tools/lldbinit` clears lldb's debuginfod server list, and the how-to loads it with `-S`.
- Why: the how-to's terminal command needs a debugger on a bootstrapped machine. Ubuntu's lldb asks `debuginfod.ubuntu.com` about every library, which took 50 to 100 seconds per launch in the dev container.
- Rejected: gdb in bootstrap, whose Ubuntu package sets `DEBUGINFOD_URLS` in every login shell; a `~/.lldbinit` written by bootstrap.

#### D14.2. `release` omits frame pointers

- Decision: "everywhere" in D14 means every preset built to debug or profile. `dev`, `cxx26`, `relwithdebinfo`, `profile`, `bench`, and the sanitizer and fuzz presets pass `-fno-omit-frame-pointer`. `release` does not. The coverage presets do not pass it either; they are unoptimized, and an unoptimized build keeps frame pointers without the flag.
- Why: `release` is the build that ships, and a frame pointer costs a register in the hot path. `profile` is `release` with frame pointers and debug info, which is the build to give `perf`.
- Rejected: the flag in `release`; the flag in the coverage presets, where it changes nothing.

### D15. Presets as the single source of truth

- Decision: `dev`, `release`, `relwithdebinfo`, `profile`, `asan`, `tsan`, `msan`, `fuzz`, `coverage`, `bench`, `cxx26`, plus `-gcc` variants and workflow presets. Same names locally and in CI. `CMakeUserPresets.json` is gitignored.
- Why: you type the same name on every platform, and green means the same thing everywhere.
- Rejected: CI flags in YAML that differ from local presets.

#### D15.1. A `-gcc` preset changes the compiler only

- Decision: the hidden `gcc` preset sets `PROJECT_COMPILER` and the Linux condition, and inherits nothing. `asan-gcc` inherits `gcc`, then `asan`.
- Why: the first parent wins, so a `gcc` preset that inherited the base options overrode its twin's.
- Rejected: repeating each twin's options in its `-gcc` variant.

#### D15.2. `release` builds for this machine's CPU

- Decision: `PROJECT_MARCH` is empty by default and `native` in `release`, so also in `bench` and `profile`. A release binary is not portable unless it is configured with a baseline such as `x86-64-v3`.
- Why: a latency-sensitive build ships and measures the fastest code for the machine it runs on, and portability is one flag away.
- Rejected: a portable default, which makes a forgotten flag a silent slowdown; a per-architecture baseline, which needs detection and differs by host.

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

#### D18.1. The tasks spell out the gcc problem matcher

- Decision: each task in `tasks.json` carries the pattern of `$gcc` inline and names no matcher by reference. The pattern also reads the check or warning name in brackets as the finding's code.
- Why: `$gcc` is defined by the Microsoft C/C++ extension, which D18 leaves out, so a task that names it reports nothing in the Problems panel. VS Code keeps one finding per position and code, so two checks on one token need the name to both show.
- Rejected: recommending the C/C++ extension for one pattern; a `declares` block to share the pattern, which VS Code reads and its `tasks.json` schema rejects.

### D19. Fast rebuilds and `make help`

- Decision: ccache when found. mold, else lld, else default on Linux; the Apple linker on macOS. A root `Makefile` with `make help`, convenience only.
- Why: rebuilds are fast without configuration, and no one looks up a preset command.
- Rejected: Makefile targets that are not a preset or target.

#### D19.1. `auto` skips a linker that cannot link the build

- Decision: `PROJECT_LINKER=auto` takes mold, then lld, only if the compiler links with it, and with LTO too when LTO is on. Otherwise it keeps the compiler's default. An explicit `mold` or `lld` that fails stops configure. `CMAKE_INTERPROCEDURAL_OPTIMIZATION` is checked with the chosen linker and stops configure in one sentence when LTO does not work.
- Why: lld cannot link GCC's LTO objects, so `release-gcc` on a machine with lld and without mold would fail at the last step.
- Rejected: a fixed order that ignores whether the linker works; `CMAKE_LINKER_TYPE`, which needs CMake 3.29 against the 3.28 floor.

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

#### D21.1. Coverage per compiler

- Decision: Clang builds with `-fprofile-instr-generate -fcoverage-mapping` and reports with `llvm-cov export -format=lcov`; GCC builds with `--coverage` and reports with `gcov` and `lcov`. Both write `build/<preset>/coverage/lcov.info` from the `coverage` target, with records for `libs/` only. The `coverage` preset uses Clang, and `coverage-gcc` exists on Linux.
- Why: each compiler's native format is exact for its own build, and one output path serves Coverage Gutters and Codecov alike.
- Rejected: gcov-format output from Clang, which is less precise and needs `lcov` on macOS; Clang only, which leaves GCC builds without coverage.

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

#### D25.1. A `plan` job decides once what runs, and `status` checks against it

- Decision: `plan` sets which job groups should run from the event, the draft flag, the labels, and the changed files. `status` requires success of each job that should run and a skip of each that should not.
- Why: a job skipped by mistake fails the one required check instead of passing it silently.
- Rejected: `status` passing on any skipped job; `paths-ignore` on the workflow, which leaves no `status` to report.

#### D25.2. Docs-only means Markdown, `docs/`, and `LICENSE`

- Decision: the compare API lists the changed files. When all match, tier one runs the pre-commit hooks only, and tier two is skipped. More than 300 files counts as a code change.
- Why: Markdown still gets markdownlint, and nothing C++ is built for a typo.
- Rejected: a third-party path filter action; a checkout with full history in `plan`.

#### D25.3. What the labels and events add

- Decision: `ci:full` runs tier two on a draft. `ci:msan` adds the `msan` job, with the instrumented libc++ cached. `ci:bench` adds `make bench-compare` against the pull request's base. macOS runs on pull request events only, never in the merge queue or on `main`.
- Why: the heavy jobs follow a reviewer's judgment, and macOS stays off the paths that repeat a reviewed PR.
- Rejected: real benchmark numbers as a gate on hosted runners.

#### D25.4. Smoke tests are test presets

- Decision: each benchmark has a CTest test labeled `bench` that runs it once, and the `bench` workflow preset ends with it. `asan-stress` and `tsan-stress` run the `stress` label, each test capped at five minutes. Install smoke is the `install_smoke` and `consumer_isolation` selftest cases, run after `release`. Every test preset writes `junit-<preset>.xml` in its build directory.
- Why: CI passes no flag a local command does not, and each job's JUnit file comes from its preset.
- Rejected: benchmark and CTest flags in YAML; stress tests in the default `asan` and `tsan` runs.

#### D25.5. The CI image is found by the hash of its inputs

- Decision: `scripts/ci-image.sh` names `ghcr.io/<owner>/<repo>-ci:<hash>`, hashing the Dockerfile and the files it copies. `plan` probes it anonymously; Linux jobs run in it when it exists and run `scripts/bootstrap.sh --ci` otherwise. The published package must be public.
- Why: an image built from other pins is never used, and a missing or private image costs minutes, not a red check.
- Rejected: a `latest` tag; registry credentials in every job.

#### D25.6. Toolchain pins stay in `bootstrap.sh`

- Decision: the setup action adds no version of its own; bootstrap's pin block holds the compilers, vcpkg, and Python tools, and bootstrap installs ccache. Actions are pinned by commit with the tag in a comment, and the selftest case `ci_pins` checks it.
- Why: CI, the dev container, and a laptop read one pin block.
- Rejected: a second pin block in the workflow, which drifts from the image.

#### D25.7. The nightly skips only its schedule, and Valgrind wraps the `dev` tests

- Decision: a scheduled run whose commit is the head of the last successful nightly runs nothing; a manual run always runs. `make valgrind` runs the `dev` unit and integration tests under CTest memcheck, and Valgrind's exit code fails a test on any error or definite leak. The instrumented libc++ is cached per full Clang version.
- Why: a failed nightly retries the next night, a person who asks for a run gets one, and Valgrind needs no build of its own.
- Rejected: a `valgrind` preset, which adds a build directory for the same binaries; a cache per Clang major, which a patch update would leave stale.

#### D25.8. The image is published only after `make dev` passes inside it

- Decision: `image.yaml` builds the image on a change to its inputs, runs `make dev` in it, and pushes on `main` and manual runs. A pull request builds and checks it without publishing.
- Why: a tool that bootstrap forgets fails before any job can pull the image.
- Rejected: `docker/build-push-action`, one more third-party action to pin for three commands.

#### D25.9. A release tag names the project version

- Decision: `v<project(VERSION)>` releases; `v<project(VERSION)>-<suffix>` makes a pre-release. `scripts/check-release-tag.sh` refuses any other tag before a build starts, naming both versions. The `package` job builds in the CI image, and a separate `publish` job holds the only write permission.
- Why: the archive's version and the tag cannot disagree, and a release candidate needs no version bump.
- Rejected: reading the version from the tag, which makes `project(VERSION)` a second source.

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
