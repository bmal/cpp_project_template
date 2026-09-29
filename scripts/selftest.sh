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
    "scaffold            new-module and new-app output builds, tests, and runs; bad names fail"
    "make_help           make help exits 0 and describes every Makefile target"
    "sample_headers      every source under libs/, apps/, tests/, benchmarks/ opens with two purpose lines"
    "stress_label        only a suite name ending in Stress moves a test from dev to stress"
    "parser_no_throw     the header-only parser contains no throw and no try"
    "fuzz_crash          a fuzz harness finds a crash planted on one specific input"
)

usage() {
    cat <<EOF
Usage: scripts/selftest.sh [case...]

Runs every case, or only the named ones, and stops at the first failure.
Prints one 'ok <case>' line per passing case.

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

for name in "${selected[@]}"; do
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
