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
#include <core/component.hpp>
#include <myproj/version.hpp>

#include <cstdio>

int main() {
    // An old-style cast and an unused variable: warnings the project's own policy would reject.
    int unused = (int)2.5;
    myproj::Counter counter;
    std::printf("%d %.*s\n", counter.increment(42).get(),
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
    output="$("${src}/build/dev/bin/myproj_cli")"
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
