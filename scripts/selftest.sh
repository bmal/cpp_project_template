#!/usr/bin/env bash
# Template selftest: lifecycle and negative cases that no preset can assert.
# Each case works on a temporary copy of the working tree and leaves the repository untouched.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# One line per case: name, then what it asserts. Run order is this order.
cases=(
    "drift_guard         a stray tests/unit/ghost/ fails configure naming it"
    "consumer_isolation  a FetchContent consumer gets no tests, apps, flags, or install rules"
    "install_smoke       an installed package builds and runs a find_package consumer"
    "version_header      the version header carries the current git commit"
    "current_build       build/current and compile_commands.json follow the last configured preset"
    "profile_symbols     the profile preset's CLI has debug info, frame pointers, and no LTO"
    "release_lto         release turns LTO on, links the CLI with -flto, and installs for a non-LTO consumer"
    "scaffold            new-module and new-app output builds, tests, and runs; bad names fail"
    "make_help           make help exits 0 and describes every Makefile target"
    "sample_headers      every source under libs/, apps/, tests/, benchmarks/ opens with two purpose lines"
    "stress_label        only a suite name ending in Stress moves a test from dev to stress"
    "lint_naming         make lint passes, then fails naming readability-identifier-naming on a camelCase function"
    "lint_presets        make lint passes on every configure preset, so code under #if is linted too, and git sees no new file"
    "parser_no_throw     the header-only parser contains no throw and no try"
    "format_roundtrip    make format-check fails on a misformatted C++ or CMake line and make format fixes it"
    "pre_commit_hooks    pre-commit run --all-files passes on the working tree"
    "clang_format_pinned .clang-format sets every key clang-format --dump-config prints, to the same value"
    "fuzz_crash          a fuzz harness finds a crash planted on one specific input"
    "fuzz_preset_only    no configure preset but fuzz has a fuzz harness target"
    "asan_overflow       a heap overflow planted in a unit test fails the asan preset with a report"
    "tsan_race           a data race planted in a unit test fails the tsan preset with a report"
    "msan_uninit         an uninitialized read planted in a unit test fails the msan preset; Linux only"
    "valgrind_leak       a leak planted in a unit test fails make valgrind with a report; Linux only"
    "coverage_report     the coverage target writes lcov.info for libs/ only, with Clang and, on Linux, GCC"
    "vscode_files        .vscode/*.json parses as JSONC, reaches builds only through build/current, and defines its own problem matcher"
    "container_files     the Dockerfile installs only through bootstrap.sh --ci, and devcontainer.json builds it and installs the git hook"
    "debugger_init       lldb is installed and loads tools/lldbinit; gdb, where installed, loads tools/gdbinit"
    "release_tag         check-release-tag.sh accepts v<project version> and its pre-releases, and names both versions otherwise"
    "ci_pins             every uses: under .github/ names a local action or a commit SHA with its version"
)

usage() {
    cat <<EOF
Usage: scripts/selftest.sh [case...]

Runs every case, or only the named ones, and stops at the first failure.
Prints one 'ok <case>' line per passing case, and 'skip <case>: <reason>' for a case this host cannot run.

Cases:
EOF
    printf '  %s\n' "${cases[@]}"
    cat <<EOF

Run one case by name:
  scripts/selftest.sh drift_guard
EOF
}

case_names=()
for entry in "${cases[@]}"; do case_names+=("${entry%% *}"); done

selected=()
for arg in "$@"; do
    case "${arg}" in
    -h | --help)
        usage
        exit 0
        ;;
    *)
        if [[ " ${case_names[*]} " != *" ${arg} "* ]]; then
            echo "selftest: unknown case '${arg}'; see scripts/selftest.sh --help" >&2
            exit 2
        fi
        selected+=("${arg}")
        ;;
    esac
done
if [ "${#selected[@]}" -eq 0 ]; then selected=("${case_names[@]}"); fi

if [ -z "${VCPKG_ROOT:-}" ] && [ -d "${root}/.vcpkg" ]; then
    export VCPKG_ROOT="${root}/.vcpkg"
fi
if [ -z "${VCPKG_ROOT:-}" ]; then
    echo "selftest: vcpkg was not found in VCPKG_ROOT or .vcpkg/; run scripts/bootstrap.sh" >&2
    exit 1
fi

work="$(mktemp -d "${TMPDIR:-/tmp}/selftest.XXXXXX")"
trap 'rm -rf "${work}"' EXIT
# Every configure shares one vcpkg install tree, so dependencies install once per run.
vcpkg_installed="${work}/vcpkg_installed"

# Copies the working tree, uncommitted changes included, into $1 as a git clone of HEAD.
copy_tree() {
    local dest="$1" file
    git clone --quiet --shared "${root}" "${dest}"
    (
        cd "${root}"
        { git diff -z --name-only HEAD && git ls-files -z --others --exclude-standard; } |
            while IFS= read -r -d '' file; do
                if [ -e "${file}" ] || [ -L "${file}" ]; then
                    mkdir -p "${dest}/$(dirname "${file}")"
                    cp -P "${file}" "${dest}/${file}"
                else
                    rm -f "${dest}/${file}"
                fi
            done
    )
}

# Configures the copy in $1 with the dev preset and extra cache arguments.
configure_dev() {
    local src="$1"
    shift
    (cd "${src}" && cmake --preset dev -DVCPKG_INSTALLED_DIR="${vcpkg_installed}" "$@")
}

# Configures a consumer project $1 into $2 with the project's toolchain and manifest from $3.
configure_consumer() {
    local consumer="$1" build="$2" src="$3"
    shift 3
    cmake -S "${consumer}" -B "${build}" -G Ninja \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
        -DCMAKE_TOOLCHAIN_FILE="${src}/cmake/toolchain.cmake" \
        -DVCPKG_MANIFEST_DIR="${src}" \
        -DVCPKG_INSTALLED_DIR="${vcpkg_installed}" \
        "$@"
}

# Builds and runs a find_package consumer $3, without LTO, of the package installed from $1 into $2.
link_installed_consumer() {
    local src="$1" prefix="$2" consumer="$3"
    mkdir -p "${consumer}"
    cat >"${consumer}/CMakeLists.txt" <<'EOF'
cmake_minimum_required(VERSION 3.28)
project(Consumer LANGUAGES CXX)
find_package(MyProj REQUIRED)
add_executable(consumer main.cpp)
target_link_libraries(consumer PRIVATE MyProj::core)
EOF
    write_consumer_main "${consumer}"
    configure_consumer "${consumer}" "${consumer}/build" "${src}" -DCMAKE_PREFIX_PATH="${prefix}"
    cmake --build "${consumer}/build"
    "${consumer}/build/consumer" | grep -q "^42 "
}

# Writes a consumer main.cpp into $1 that prints the version it was built against.
write_consumer_main() {
    cat >"$1/main.cpp" <<'EOF'
#include <core/counter.hpp>
#include <myproj/version.hpp>

#include <cstdio>

int main() {
    // An old-style cast and an unused variable: warnings the project's own policy would reject.
    int unused = (int)2.5;
    myproj::core::Counter counter;
    counter.add(42);
    std::printf("%lld %.*s\n", static_cast<long long>(counter.value()),
                static_cast<int>(myproj::version_string.size()), myproj::version_string.data());
    return 0;
}
EOF
}

case_drift_guard() {
    local src="${work}/drift_guard"
    copy_tree "${src}"
    mkdir -p "${src}/tests/unit/ghost"
    if configure_dev "${src}" -DPROJECT_BUILD_APPS=OFF >"${work}/drift_guard.out" 2>&1; then
        cat "${work}/drift_guard.out"
        echo "configure succeeded with a stray tests/unit/ghost/"
        return 1
    fi
    cat "${work}/drift_guard.out"
    grep -q "tests/unit/ghost has no matching module" "${work}/drift_guard.out"
}

case_consumer_isolation() {
    local src="${work}/consumer_isolation" consumer="${work}/consumer_isolation-consumer"
    local build="${consumer}/build"
    copy_tree "${src}"
    mkdir -p "${consumer}"
    cat >"${consumer}/CMakeLists.txt" <<EOF
cmake_minimum_required(VERSION 3.28)
project(Consumer LANGUAGES CXX)
include(FetchContent)
FetchContent_Declare(MyProj SOURCE_DIR "${src}")
FetchContent_MakeAvailable(MyProj)
enable_testing()
add_executable(consumer main.cpp)
target_compile_options(consumer PRIVATE -Wall -Wextra)
target_link_libraries(consumer PRIVATE myproj::core)
install(TARGETS consumer)
EOF
    write_consumer_main "${consumer}"
    configure_consumer "${consumer}" "${build}" "${src}"
    cmake --build "${build}"
    "${build}/consumer"

    if grep -q -- "-Werror" "${build}/compile_commands.json"; then
        echo "-Werror reached the consumer build"
        return 1
    fi
    # The consumer asked for -Wall -Wextra only; any other -W flag came from the project.
    local own_command
    own_command="$(grep '"command"' "${build}/compile_commands.json" | grep -F "/consumer_isolation-consumer/main.cpp")"
    [ -n "${own_command}" ]
    if sed 's/ -Wall//; s/ -Wextra//' <<<"${own_command}" | grep -q -- " -W"; then
        echo "the project's warning flags reached the consumer's own sources"
        return 1
    fi
    cmake --install "${build}" --prefix "${consumer}/prefix"
    if [ -n "$(find "${consumer}/prefix" -type f ! -name consumer)" ]; then
        find "${consumer}/prefix" -type f ! -name consumer
        echo "the consumer's install tree received the project's files"
        return 1
    fi
    if ! ctest --test-dir "${build}" -N | grep -q "Total Tests: 0"; then
        echo "the consumer build registered the project's tests"
        return 1
    fi
    if [ -n "$(find "${build}" -name myproj_cli -type f)" ]; then
        echo "the consumer build built the project's apps"
        return 1
    fi
}

case_install_smoke() {
    local src="${work}/install_smoke" consumer="${work}/install_smoke-consumer"
    local prefix="${work}/install_smoke-prefix"
    copy_tree "${src}"
    configure_dev "${src}" -DPROJECT_BUILD_TESTS=OFF -DPROJECT_BUILD_APPS=OFF
    (cd "${src}" && cmake --build --preset dev)
    cmake --install "${src}/build/dev" --prefix "${prefix}"
    link_installed_consumer "${src}" "${prefix}" "${consumer}"
}

case_version_header() {
    local src="${work}/version_header" commit output
    copy_tree "${src}"
    commit="$(git -C "${root}" rev-parse HEAD)"
    configure_dev "${src}" -DPROJECT_BUILD_TESTS=OFF
    (cd "${src}" && cmake --build --preset dev --target myproj_cli)
    output="$("${src}/build/dev/bin/myproj_cli" </dev/null)"
    echo "${output}"
    # The CLI prints "myproj <version> (<short commit>[-dirty])" on its first line.
    local short
    short="$(sed -n '1s/^myproj [^ ]* (\([0-9a-f]*\).*/\1/p' <<<"${output}")"
    if [ -z "${short}" ] || [[ "${commit}" != "${short}"* ]]; then
        echo "expected a prefix of ${commit}, got '${short}'"
        return 1
    fi
}

# Fails naming preset $3 when git status of the copy $1 differs from the snapshot $2.
expect_git_unchanged() {
    if [ "$(git -C "$1" status --porcelain)" != "$2" ]; then
        diff <(echo "$2") <(git -C "$1" status --porcelain) || true
        echo "cmake --preset $3 left files git does not ignore"
        return 1
    fi
}

# Configures preset $2 in the copy $1, then checks that build/current and the root
# compile_commands.json point at build/$2, with $3 in its commands, and that git sees nothing new.
expect_current_build() {
    local src="$1" preset="$2" flag="$3" target before
    # The copy already shows the uncommitted changes of the working tree.
    before="$(git -C "${src}" status --porcelain)"
    (cd "${src}" && cmake --preset "${preset}" -DVCPKG_INSTALLED_DIR="${vcpkg_installed}")
    target="$(readlink "${src}/build/current")"
    if [[ "${target}" != */build/"${preset}" ]]; then
        echo "after cmake --preset ${preset}, build/current points at '${target}'"
        return 1
    fi
    if ! grep -q -- "${flag}" "${src}/compile_commands.json"; then
        echo "after cmake --preset ${preset}, the root compile_commands.json has no ${flag}"
        return 1
    fi
    expect_git_unchanged "${src}" "${before}" "${preset}"
}

case_current_build() {
    local src="${work}/current_build"
    copy_tree "${src}"
    expect_current_build "${src}" asan "-fsanitize=address"
    expect_current_build "${src}" dev "-O0"
    if grep -q -- "-fsanitize=address" "${src}/compile_commands.json"; then
        echo "after cmake --preset dev, the root compile_commands.json still has -fsanitize=address"
        return 1
    fi
    # The personal preset from docs/how-to/personal-presets.md, copied from the page itself.
    awk '/^```json$/ { on = 1; next } /^```$/ { on = 0 } on' "${root}/docs/how-to/personal-presets.md" \
        >"${src}/CMakeUserPresets.json"
    if ! git -C "${src}" check-ignore -q CMakeUserPresets.json; then
        echo "git does not ignore CMakeUserPresets.json"
        return 1
    fi
    expect_current_build "${src}" dev-mine "-O0"
}

# Succeeds when the binary $1 carries debug information for the translation unit $2.
has_debug_info() {
    local binary="$1" source="$2" names
    # Captured first: grep -q ends the pipe early, and pipefail would report the writer's SIGPIPE.
    if [ "$(uname -s)" = Darwin ]; then
        # macOS keeps DWARF in the object files; the binary's debug map names each one.
        names="$(nm -ap "${binary}" | grep ' OSO ')" || return 1
        grep -qF "${source}.o" <<<"${names}"
    else
        names="$(readelf --debug-dump=info "${binary}" | grep 'DW_AT_name')" || return 1
        grep -qF "${source}" <<<"${names}"
    fi
}

# Prints the compile command of every project source under libs/ and apps/ in the build $1.
own_compile_commands() {
    grep '"command"' "$1/compile_commands.json" | grep -E '/(libs|apps)/[^ ]*\.cpp"'
}

case_profile_symbols() {
    local src="${work}/profile_symbols" build command
    copy_tree "${src}"
    build="${src}/build/profile"
    (cd "${src}" && cmake --preset profile -DVCPKG_INSTALLED_DIR="${vcpkg_installed}" -DPROJECT_BUILD_TESTS=OFF)
    (cd "${src}" && cmake --build --preset profile --target myproj_cli)
    if ! has_debug_info "${build}/bin/myproj_cli" main.cpp; then
        echo "the profile build of myproj_cli has no debug information for main.cpp"
        return 1
    fi
    [ -n "$(own_compile_commands "${build}")" ]
    while IFS= read -r command; do
        if [[ " ${command} " != *" -fno-omit-frame-pointer "* || " ${command} " != *" -g "* ]]; then
            echo "a profile compile command lacks -g or -fno-omit-frame-pointer: ${command}"
            return 1
        fi
    done < <(own_compile_commands "${build}")
    if ninja -C "${build}" -t commands myproj_cli | tail -n 1 | grep -q -- "-flto"; then
        echo "the profile build links myproj_cli with LTO, which hides inlined frames from perf"
        return 1
    fi
}

case_release_lto() {
    local src="${work}/release_lto" build
    copy_tree "${src}"
    build="${src}/build/release"
    (cd "${src}" && cmake --preset release -DVCPKG_INSTALLED_DIR="${vcpkg_installed}" -DPROJECT_BUILD_TESTS=OFF)
    (cd "${src}" && cmake --build --preset release --target myproj_cli)
    if ! grep -Eq '^CMAKE_INTERPROCEDURAL_OPTIMIZATION(:[A-Z]+)?=ON$' "${build}/CMakeCache.txt"; then
        grep CMAKE_INTERPROCEDURAL_OPTIMIZATION "${build}/CMakeCache.txt" || true
        echo "the release cache does not turn CMAKE_INTERPROCEDURAL_OPTIMIZATION on"
        return 1
    fi
    ninja -C "${build}" -t commands myproj_cli | tail -n 1 | tee "${work}/release_lto.link"
    if ! grep -q -- "-flto" "${work}/release_lto.link"; then
        echo "the release link line of myproj_cli has no -flto"
        return 1
    fi
    echo "a=1" | "${build}/bin/myproj_cli"
    # A consumer that links without LTO must still link the installed LTO archives.
    cmake --install "${build}" --prefix "${work}/release_lto-prefix"
    link_installed_consumer "${src}" "${work}/release_lto-prefix" "${work}/release_lto-consumer"
}

case_scaffold() {
    local src="${work}/scaffold"
    copy_tree "${src}"
    if "${src}/scripts/new-module.sh" 9bad; then
        echo "new-module.sh accepted the name 9bad"
        return 1
    fi
    "${src}/scripts/new-module.sh" widget
    if "${src}/scripts/new-module.sh" widget; then
        echo "new-module.sh overwrote the existing module widget"
        return 1
    fi
    if "${src}/scripts/new-app.sh" test; then
        echo "new-app.sh accepted test, a target name CMake reserves"
        return 1
    fi
    "${src}/scripts/new-app.sh" tool
    configure_dev "${src}"
    (cd "${src}" && cmake --build --preset dev)
    (cd "${src}" && ctest --preset dev -L widget) | tee "${work}/scaffold.ctest"
    grep -Eq "100% tests passed(, 0 tests failed)? out of 1$" "${work}/scaffold.ctest"
    "${src}/build/dev/bin/tool"
}

case_make_help() {
    local targets target
    make -C "${root}" --no-print-directory help | tee "${work}/make_help.out"
    # Every rule target; variable assignments with := and the workflow/% pattern are not targets.
    targets="$(sed -n '/^[A-Za-z0-9][A-Za-z0-9_.-]*:=/d; s/^\([A-Za-z0-9][A-Za-z0-9_.-]*\):.*/\1/p' "${root}/Makefile")"
    [ -n "${targets}" ]
    for target in ${targets}; do
        if ! grep -Eq "^  ${target} +[^ ]" "${work}/make_help.out"; then
            echo "make help has no description for target ${target}"
            return 1
        fi
    done
}

case_sample_headers() {
    local file first second missing=0
    while IFS= read -r -d '' file; do
        case "${file}" in
        *.cpp | *.hpp | *.cmake | */CMakeLists.txt) ;;
        *) continue ;;
        esac
        [ -f "${root}/${file}" ] || continue
        first="$(sed -n 1p "${root}/${file}")"
        second="$(sed -n 2p "${root}/${file}")"
        # Two C++ line comments, or two CMake comments that are not directives such as #pragma.
        if [[ "${first}" != "// "?* && "${first}" != "# "?* ]] ||
            [[ "${second}" != "// "?* && "${second}" != "# "?* ]]; then
            echo "${file} does not open with a two-line purpose comment: '${first}'"
            missing=1
        elif grep -Eqi "copyright|license|spdx" <<<"${first}"; then
            echo "${file} opens with a license line, not its purpose: '${first}'"
            missing=1
        fi
    done < <(git -C "${root}" ls-files -z --cached --others --exclude-standard -- \
        libs apps tests benchmarks)
    [ "${missing}" -eq 0 ]
}

case_stress_label() {
    local src="${work}/stress_label" name
    copy_tree "${src}"
    cat >"${src}/tests/unit/parser/test_label_probe.cpp" <<'EOF'
// Selftest: every shape of test name that does or does not belong to the stress label.
// Only a suite name that ends in Stress counts.
#include <gtest/gtest.h>

template <typename T>
class TypedProbeStress : public testing::Test {};
using ProbeTypes = testing::Types<int, long>;
TYPED_TEST_SUITE(TypedProbeStress, ProbeTypes);
TYPED_TEST(TypedProbeStress, Runs) { SUCCEED(); }

class ParamProbeStress : public testing::TestWithParam<int> {};
TEST_P(ParamProbeStress, Runs) { SUCCEED(); }
INSTANTIATE_TEST_SUITE_P(Probe, ParamProbeStress, testing::Values(1));

TEST(PlainProbeStress, Runs) { SUCCEED(); }

class QuickProbe : public testing::TestWithParam<int> {};
TEST_P(QuickProbe, SurvivesStress) { SUCCEED(); }
INSTANTIATE_TEST_SUITE_P(Probe, QuickProbe, testing::Values(1));

TEST(StressfulProbe, Runs) { SUCCEED(); }
TEST(QuickPlainProbe, SurvivesStress) { SUCCEED(); }
EOF
    configure_dev "${src}" -DPROJECT_BUILD_APPS=OFF
    (cd "${src}" && cmake --build --preset dev --target parser_unit_tests)
    (cd "${src}" && ctest --preset dev -N) | tee "${work}/stress_label.dev"
    (cd "${src}" && ctest --preset stress -N) | tee "${work}/stress_label.stress"
    for name in "TypedProbeStress.Runs<int>" "TypedProbeStress.Runs<long>" "Probe/ParamProbeStress.Runs/" \
        "PlainProbeStress.Runs"; do
        if ! grep -qF "${name}" "${work}/stress_label.stress"; then
            echo "${name} has a suite ending in Stress but the stress preset does not run it"
            return 1
        fi
        if grep -qF "${name}" "${work}/stress_label.dev"; then
            echo "${name} has a suite ending in Stress but the dev preset runs it"
            return 1
        fi
    done
    for name in "Probe/QuickProbe.SurvivesStress/" "StressfulProbe.Runs" \
        "QuickPlainProbe.SurvivesStress"; do
        if ! grep -qF "${name}" "${work}/stress_label.dev"; then
            echo "${name} is not in a Stress suite but the dev preset leaves it out"
            return 1
        fi
        if grep -qF "${name}" "${work}/stress_label.stress"; then
            echo "${name} is not in a Stress suite but the stress preset runs it"
            return 1
        fi
    done
}

case_lint_naming() {
    local src="${work}/lint_naming"
    copy_tree "${src}"
    configure_dev "${src}"
    make -C "${src}" --no-print-directory lint
    cat >"${src}/libs/core/src/lint_probe.cpp" <<'EOF'
// Selftest: a function named in camelCase, which D13 forbids.
// Nothing calls it; only the lint target reads it.
namespace myproj::core {

int lintProbe() { return 0; }

} // namespace myproj::core
EOF
    if make -C "${src}" --no-print-directory lint >"${work}/lint_naming.out" 2>&1; then
        cat "${work}/lint_naming.out"
        echo "make lint passed a camelCase function"
        return 1
    fi
    cat "${work}/lint_naming.out"
    grep -q "lint_probe.cpp:.*invalid case style for function 'lintProbe'.*readability-identifier-naming" \
        "${work}/lint_naming.out"
}

case_lint_presets() {
    local src="${work}/lint_presets" preset checked=0 before
    copy_tree "${src}"
    before="$(git -C "${src}" status --porcelain)"
    for preset in $(cd "${src}" && cmake --list-presets=configure | sed -n 's/^ *"\([^"]*\)".*/\1/p'); do
        if [ "${preset}" = msan ] && [ -n "$(skip_reason msan_uninit)" ]; then
            echo "msan: skipped, $(skip_reason msan_uninit)"
            continue
        fi
        (cd "${src}" && cmake --preset "${preset}" -DVCPKG_INSTALLED_DIR="${vcpkg_installed}")
        if ! (cd "${src}" && cmake --build --preset "${preset}" --target lint); then
            echo "make lint PRESET=${preset} reports findings that the dev preset does not compile"
            return 1
        fi
        expect_git_unchanged "${src}" "${before}" "${preset}"
        echo "${preset}: lint is clean"
        checked=$((checked + 1))
    done
    [ "${checked}" -gt 0 ]
}

case_format_roundtrip() {
    local src="${work}/format_roundtrip"
    copy_tree "${src}"
    make -C "${src}" --no-print-directory format-check
    # One C++ line with the pointer bound to the name, and one CMake line indented by five spaces.
    sed -i.orig 's/^int main() {$/int main() { int *probe = nullptr; (void)probe;/' "${src}/apps/myproj_cli/main.cpp"
    sed -i.orig 's/^  project_add_lint_target()$/     project_add_lint_target()/' "${src}/CMakeLists.txt"
    rm "${src}/apps/myproj_cli/main.cpp.orig" "${src}/CMakeLists.txt.orig"
    if make -C "${src}" --no-print-directory format-check >"${work}/format_roundtrip.out" 2>&1; then
        cat "${work}/format_roundtrip.out"
        echo "make format-check passed a misformatted C++ and CMake line"
        return 1
    fi
    cat "${work}/format_roundtrip.out"
    grep -q "main.cpp" "${work}/format_roundtrip.out"
    grep -q "CMakeLists.txt" "${work}/format_roundtrip.out"
    make -C "${src}" --no-print-directory format
    make -C "${src}" --no-print-directory format-check
    grep -q "int\* probe = nullptr;" "${src}/apps/myproj_cli/main.cpp"
    grep -q "^  project_add_lint_target()$" "${src}/CMakeLists.txt"
}

case_pre_commit_hooks() {
    local src="${work}/pre_commit_hooks"
    copy_tree "${src}"
    # --all-files reads the index, so new files in the working tree must be staged.
    git -C "${src}" add -A
    (cd "${src}" && "$(pipx environment --value PIPX_BIN_DIR)/pre-commit" run --all-files --show-diff-on-failure)
}

case_clang_format_pinned() {
    local clang_format
    clang_format="$("${root}/scripts/format.sh" --print-clang-format)"
    [ -n "${clang_format}" ]
    # Comments and blank lines are not keys; everything else must match line for line.
    diff <(grep -Ev '^(#|$)' "${root}/.clang-format") \
        <(cd "${root}" && "${clang_format}" --dump-config | grep -Ev '^(#|$)')
}

case_parser_no_throw() {
    # NO_EXCEPTIONS cannot add -fno-exceptions to a module without sources, so this checks the text.
    if grep -rnwE "throw|try" "${root}/libs/parser"; then
        echo "libs/parser must report errors in std::expected, never throw"
        return 1
    fi
}

case_fuzz_crash() {
    local src="${work}/fuzz_crash" out="${work}/fuzz_crash-out"
    copy_tree "${src}"
    mkdir -p "${src}/tests/fuzz/corpus/planted_fuzz" "${out}"
    printf 'seed' >"${src}/tests/fuzz/corpus/planted_fuzz/seed"
    cat >"${src}/tests/fuzz/planted_fuzz.cpp" <<'EOF'
// Selftest: crashes on inputs that start with "FUZ", which no seed does.
#include <cstddef>
#include <cstdint>
#include <cstdlib>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    if (size >= 3 && data[0] == 'F' && data[1] == 'U' && data[2] == 'Z') {
        std::abort();
    }
    return 0;
}
EOF
    echo 'project_add_fuzz_target(NAME planted_fuzz)' >>"${src}/tests/fuzz/CMakeLists.txt"
    (cd "${src}" && cmake --preset fuzz -DVCPKG_INSTALLED_DIR="${vcpkg_installed}")
    (cd "${src}" && cmake --build --preset fuzz --target planted_fuzz)
    if "${src}/build/fuzz/bin/planted_fuzz" -max_total_time=30 -artifact_prefix="${out}/" \
        "${src}/tests/fuzz/corpus/planted_fuzz" >"${work}/fuzz_crash.out" 2>&1; then
        tail -n 20 "${work}/fuzz_crash.out"
        echo "the fuzzer did not find the planted crash within 30 seconds"
        return 1
    fi
    tail -n 20 "${work}/fuzz_crash.out"
    grep -q "ERROR: libFuzzer: deadly signal" "${work}/fuzz_crash.out"
    # The crash file is the input itself, which must be the planted one.
    head -c 3 "${out}"/crash-* | grep -q "^FUZ"
}

case_fuzz_preset_only() {
    local src="${work}/fuzz_preset_only" preset out checked=0
    copy_tree "${src}"
    for preset in $(cd "${src}" && cmake --list-presets=configure | sed -n 's/^ *"\([^"]*\)".*/\1/p'); do
        if [ "${preset}" = fuzz ]; then continue; fi
        if [ "${preset}" = msan ] && [ -n "$(skip_reason msan_uninit)" ]; then
            echo "msan: skipped, $(skip_reason msan_uninit)"
            continue
        fi
        out="${work}/fuzz_preset_only-${preset}.out"
        (cd "${src}" && cmake --preset "${preset}" -DVCPKG_INSTALLED_DIR="${vcpkg_installed}")
        if (cd "${src}" && cmake --build --preset "${preset}" --target parser_fuzz) >"${out}" 2>&1 ||
            ! grep -q "unknown target 'parser_fuzz'" "${out}"; then
            cat "${out}"
            echo "the ${preset} preset has a parser_fuzz target; only the fuzz preset may build harnesses"
            return 1
        fi
        echo "${preset}: no parser_fuzz target"
        checked=$((checked + 1))
    done
    if [ "${checked}" -eq 0 ]; then
        echo "found no configure preset other than fuzz to check"
        return 1
    fi
}

# Adds the unit test source $2 to the parser tests of the copy in $1, then runs preset $3 on it.
# Passes only when the preset fails the test and prints the sanitizer report $4.
expect_sanitizer_report() {
    local src="$1" test_source="$2" preset="$3" report="$4" out="${work}/${3}_planted.out"
    cp "${test_source}" "${src}/tests/unit/parser/test_planted.cpp"
    (cd "${src}" && cmake --preset "${preset}" -DVCPKG_INSTALLED_DIR="${vcpkg_installed}")
    (cd "${src}" && cmake --build --preset "${preset}" --target parser_unit_tests)
    if (cd "${src}" && ctest --preset "${preset}" -R '^Planted\.') >"${out}" 2>&1; then
        cat "${out}"
        echo "the ${preset} preset passed a test with a planted bug"
        return 1
    fi
    if ! grep -A 12 "${report}" "${out}"; then
        cat "${out}"
        echo "the ${preset} preset failed without printing '${report}'"
        return 1
    fi
    # A frame must name the planted test, or the report does not say where the bug is.
    if ! grep -Eq "#[0-9]+ .*(Planted_|test_planted\.cpp)" "${out}"; then
        echo "the ${preset} report names no function; llvm-symbolizer is missing, run scripts/bootstrap.sh"
        return 1
    fi
}

case_asan_overflow() {
    local src="${work}/asan_overflow"
    copy_tree "${src}"
    cat >"${work}/asan_overflow.cpp" <<'EOF'
// Selftest: writes one element past a heap array, which only AddressSanitizer notices.
// The index is volatile so the compiler cannot prove the overflow and warn at build time.
#include <gtest/gtest.h>

#include <cstddef>
#include <memory>

TEST(Planted, HeapOverflow) {
    volatile std::size_t past_end = 4;
    const auto values = std::make_unique<int[]>(4);
    values[past_end] = 1;
    EXPECT_EQ(values[0], 0);
}
EOF
    expect_sanitizer_report "${src}" "${work}/asan_overflow.cpp" asan \
        "ERROR: AddressSanitizer: heap-buffer-overflow"
}

case_tsan_race() {
    local src="${work}/tsan_race"
    copy_tree "${src}"
    cat >"${work}/tsan_race.cpp" <<'EOF'
// Selftest: two threads write one int without synchronization, which only ThreadSanitizer notices.
// Nothing is checked, because the race itself is the bug under test.
#include <gtest/gtest.h>

#include <thread>

TEST(Planted, DataRace) {
    int shared = 0;
    std::thread writer([&shared] { shared = 1; });
    shared = 2;
    writer.join();
    SUCCEED();
}
EOF
    expect_sanitizer_report "${src}" "${work}/tsan_race.cpp" tsan "WARNING: ThreadSanitizer: data race"
}

case_msan_uninit() {
    local src="${work}/msan_uninit"
    copy_tree "${src}"
    cat >"${work}/msan_uninit.cpp" <<'EOF'
// Selftest: branches on a heap int that was never written, which only MemorySanitizer notices.
// The pointer is volatile so the compiler cannot prove the read uninitialized and warn at build time.
#include <gtest/gtest.h>

#include <memory>

TEST(Planted, UninitializedRead) {
    const std::unique_ptr<int> value(new int);
    int* volatile alias = value.get();
    if (*alias > 0) {
        SUCCEED();
    }
}
EOF
    expect_sanitizer_report "${src}" "${work}/msan_uninit.cpp" msan \
        "WARNING: MemorySanitizer: use-of-uninitialized-value"
}

case_valgrind_leak() {
    local src="${work}/valgrind_leak" out="${work}/valgrind_leak.out"
    copy_tree "${src}"
    cat >"${src}/tests/unit/parser/test_planted.cpp" <<'EOF'
// Selftest: allocates and never frees, which Valgrind memcheck reports as definitely lost.
// The pointer is volatile so the compiler cannot drop the allocation.
#include <gtest/gtest.h>

TEST(Planted, Leak) {
    int* volatile leaked = new int[4];
    EXPECT_NE(leaked, nullptr);
}
EOF
    configure_dev "${src}"
    if make -C "${src}" valgrind >"${out}" 2>&1; then
        cat "${out}"
        echo "make valgrind passed a test with a planted leak"
        return 1
    fi
    if ! grep -E "^Memory Leak - [0-9]+" "${out}"; then
        cat "${out}"
        echo "make valgrind failed without reporting a memory leak"
        return 1
    fi
}

# Runs the coverage preset $2 in the copy $1 and checks its lcov.info names libs/ and no dependency.
expect_lcov() {
    local src="$1" preset="$2" info libs
    info="${src}/build/${preset}/coverage/lcov.info"
    # Records carry the normalized source path, which a TMPDIR ending in / would not match.
    libs="/$(basename "${src}")/libs"
    (cd "${src}" && cmake --preset "${preset}" -DVCPKG_INSTALLED_DIR="${vcpkg_installed}")
    (cd "${src}" && cmake --build --preset "${preset}")
    (cd "${src}" && ctest --preset "${preset}")
    (cd "${src}" && cmake --build --preset "${preset}" --target coverage)
    for module in core parser; do
        if ! grep -q "^SF:.*${libs}/${module}/" "${info}"; then
            echo "${preset}: ${info} has no record for libs/${module}"
            return 1
        fi
    done
    if grep "^SF:" "${info}" | grep -v "^SF:.*${libs}/"; then
        echo "${preset}: ${info} has records outside libs/, listed above"
        return 1
    fi
    # Coverage Gutters reads this path, which follows the last configured preset.
    [ "${src}/build/current/coverage/lcov.info" -ef "${info}" ]
}

case_coverage_report() {
    local src="${work}/coverage_report"
    copy_tree "${src}"
    expect_lcov "${src}" coverage
    if [ "$(uname -s)" = Linux ]; then
        expect_lcov "${src}" coverage-gcc
    fi
}

# Parses each argument as JSONC and prints it as JSON, one document per line.
parse_jsonc() {
    python3 - "$@" <<'PY'
import json, re, sys
for path in sys.argv[1:]:
    text = open(path).read()
    # JSONC: drop // comments outside strings, then trailing commas.
    text = re.sub(r'("(?:\\.|[^"\\])*")|//[^\n]*', lambda m: m.group(1) or "", text)
    print(json.dumps(json.loads(re.sub(r",(\s*[}\]])", r"\1", text))))
PY
}

case_vscode_files() {
    parse_jsonc "${root}"/.vscode/{extensions,launch,settings,tasks}.json | python3 -c '
import json, re, sys

_, launch, settings, tasks = (json.loads(line) for line in sys.stdin)

def strings(node):
    if isinstance(node, str):
        yield node
    elif isinstance(node, dict):
        for child in node.values():
            yield from strings(child)
    elif isinstance(node, list):
        for child in node:
            yield from strings(child)

for text in strings([launch, settings, tasks]):
    if re.search(r"build/(?!current(/|$))", text):
        sys.exit(".vscode names a build directory other than build/current: " + text)
# With either setting gone, the extension searches every build directory.
for key in ("testMate.cpp.test.executables", "coverage-gutters.coverageBaseDir"):
    if not settings.get(key, "").startswith("build/current/"):
        sys.exit("settings.json does not point " + key + " at build/current/")
# A matcher named by reference belongs to an extension; none in extensions.json defines one.
for task in tasks["tasks"]:
    if not isinstance(task.get("problemMatcher"), dict):
        sys.exit("task " + task["label"] + " does not define its problem matcher in tasks.json")
'
}

case_container_files() {
    # Tool versions live only in bootstrap.sh; a package installed here would drift from it.
    # Options may stand between the package manager and its install command.
    if grep -nE "(apt|apt-get|brew|pip|pipx)[[:space:]]+([^&|;]*[[:space:]])?install([[:space:]]|$)" \
        "${root}/Dockerfile"; then
        echo "Dockerfile installs a package itself, listed above; add it to scripts/bootstrap.sh"
        return 1
    fi
    if ! grep -q "scripts/bootstrap.sh --ci" "${root}/Dockerfile"; then
        echo "Dockerfile does not run scripts/bootstrap.sh --ci"
        return 1
    fi
    parse_jsonc "${root}/.devcontainer/devcontainer.json" "${root}/.vscode/extensions.json" |
        python3 -c '
import json, sys
container, editor = (json.loads(line) for line in sys.stdin)
if container["build"]["dockerfile"] != "../Dockerfile":
    sys.exit("devcontainer.json does not build the root Dockerfile")
if container["customizations"]["vscode"]["extensions"] != editor["recommendations"]:
    sys.exit("devcontainer.json extensions differ from .vscode/extensions.json")
# bootstrap.sh --ci runs with no checkout, so the hook can only be installed after create.
if "pre-commit install" not in container["postCreateCommand"]:
    sys.exit("devcontainer.json does not install the pre-commit hook after create")
'
}

case_debugger_init() {
    # docs/how-to/debug-a-test.md runs lldb by its plain name with -S. No program is loaded here,
    # so each breakpoint stays pending, which is what loading the file from ~/.lldbinit does.
    if ! command -v lldb >/dev/null; then
        echo "lldb is not installed; run scripts/bootstrap.sh"
        return 1
    fi
    lldb --batch -S "${root}/tools/lldbinit" 2>&1 | tee "${work}/debugger_init.lldb"
    if grep "^error:" "${work}/debugger_init.lldb"; then
        echo "this lldb rejects a command of tools/lldbinit, listed above"
        return 1
    fi
    grep -q "^Breakpoint 2: " "${work}/debugger_init.lldb"
    # Bootstrap leaves gdb out: on Ubuntu it turns on symbol downloads for every debugger.
    if command -v gdb >/dev/null; then
        gdb -batch -x "${root}/tools/gdbinit" 2>&1 | tee "${work}/debugger_init.gdb"
        grep -q "^Breakpoint 2 (__ubsan_on_report) pending" "${work}/debugger_init.gdb"
    fi
}

case_ci_pins() {
    # A tag can be moved to other code after review; a commit cannot.
    if grep -rnE "uses:" "${root}/.github" |
        grep -vE "uses: (\./[^ ]+|[^ @]+@[0-9a-f]{40} # v[0-9][^ ]*)$"; then
        echo "an action above is not pinned; use <action>@<commit SHA> # <version tag>"
        return 1
    fi
}

case_release_tag() {
    local version tag
    version="$(sed -n 's/^project([^ ]* VERSION \([0-9.]*\).*/\1/p' "${root}/CMakeLists.txt")"
    for tag in "v${version}" "v${version}-rc.1"; do
        "${root}/scripts/check-release-tag.sh" "${tag}"
    done
    for tag in v0.0.0-test "${version}" "v${version}.1" "v${version}--"; do
        if "${root}/scripts/check-release-tag.sh" "${tag}" 2>"${work}/release_tag.err"; then
            echo "check-release-tag.sh accepted ${tag} for project version ${version}"
            return 1
        fi
        cat "${work}/release_tag.err"
        if [ "${tag}" = v0.0.0-test ] && ! grep -q "0.0.0.*${version}" "${work}/release_tag.err"; then
            echo "the rejection of ${tag} does not name both 0.0.0 and ${version}"
            return 1
        fi
    done
}

# Prints why a case cannot run on this host, or nothing when it can.
skip_reason() {
    case "$1" in
    pre_commit_hooks)
        if ! command -v pipx >/dev/null ||
            [ ! -x "$(pipx environment --value PIPX_BIN_DIR)/pre-commit" ]; then
            echo "pre-commit is not installed; run scripts/bootstrap.sh"
        fi
        ;;
    valgrind_leak)
        if [ "$(uname -s)" != Linux ]; then
            echo "Valgrind runs on Linux only"
        elif ! command -v valgrind >/dev/null; then
            echo "valgrind is not installed; run scripts/bootstrap.sh"
        fi
        ;;
    msan_uninit)
        if [ "$(uname -s)" != Linux ]; then
            echo "MemorySanitizer runs on Linux only"
        elif ! "${root}/scripts/build-msan-libcxx.sh" --check >/dev/null 2>&1; then
            echo "no instrumented libc++; run scripts/build-msan-libcxx.sh"
        fi
        ;;
    esac
}

for name in "${selected[@]}"; do
    reason="$(skip_reason "${name}")"
    if [ -n "${reason}" ]; then
        echo "skip ${name}: ${reason}"
        continue
    fi
    log="${work}/${name}.log"
    # Not an if condition, because errexit does not apply inside one.
    set +e
    (
        set -e
        "case_${name}"
    ) >"${log}" 2>&1
    status=$?
    set -e
    if [ "${status}" -eq 0 ]; then
        echo "ok ${name}"
    else
        echo "FAIL ${name}"
        tail -n 40 "${log}" | sed 's/^/  /'
        exit 1
    fi
done
