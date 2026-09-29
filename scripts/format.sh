#!/usr/bin/env bash
# Formats C++ with clang-format and CMake with gersemi, in place or as a check that changes nothing.
# make format, make format-check, and the pre-commit hooks all call this script.
set -euo pipefail

usage() {
    cat <<EOF
Usage: scripts/format.sh [--check] [file...]
       scripts/format.sh --print-clang-format

Formats the given files, or every C++ and CMake file git does not ignore. --check changes nothing and
exits 1 when a file is not formatted. --print-clang-format prints the clang-format it uses.
EOF
}

check=0
print_clang_format=0
files=()
for arg in "$@"; do
    case "${arg}" in
    -h | --help)
        usage
        exit 0
        ;;
    --check) check=1 ;;
    --print-clang-format) print_clang_format=1 ;;
    *) files+=("${arg}") ;;
    esac
done

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${root}"
if [ "${#files[@]}" -eq 0 ]; then
    while IFS= read -r -d '' file; do
        [ -f "${file}" ] && files+=("${file}")
    done < <(git ls-files -z --cached --others --exclude-standard -- \
        '*.cpp' '*.hpp' '*.h' 'CMakeLists.txt' '*/CMakeLists.txt' '*.cmake')
    if [ "${#files[@]}" -eq 0 ]; then
        echo "format: no C++ or CMake files found; run it inside a git checkout" >&2
        exit 1
    fi
fi

cxx_files=()
cmake_files=()
for file in "${files[@]}"; do
    case "${file}" in
    *.cpp | *.hpp | *.h) cxx_files+=("${file}") ;;
    *.cmake | CMakeLists.txt | */CMakeLists.txt) cmake_files+=("${file}") ;;
    esac
done

# The LLVM major scripts/bootstrap.sh installs; .clang-format is the dump of that clang-format.
# shellcheck disable=SC2016 # The pattern matches a literal ${CLANG_VERSION:-22}.
pinned_major="$(sed -n 's/^CLANG_VERSION="\${CLANG_VERSION:-\([0-9]*\)}"$/\1/p' scripts/bootstrap.sh)"

# Prints the pinned versioned clang-format, else the Homebrew LLVM one, else the unversioned one.
find_clang_format() {
    local candidate
    if command -v "clang-format-${pinned_major}"; then
        return
    fi
    for candidate in /opt/homebrew/opt/llvm/bin/clang-format /usr/local/opt/llvm/bin/clang-format; do
        if [ -x "${candidate}" ]; then
            echo "${candidate}"
            return
        fi
    done
    command -v clang-format || true
}

# Prints gersemi from PATH or from pipx's directory, which may not be on PATH yet.
find_gersemi() {
    local bin_dir
    if command -v gersemi; then
        return
    fi
    bin_dir="$(pipx environment --value PIPX_BIN_DIR 2>/dev/null || true)"
    if [ -n "${bin_dir}" ] && [ -x "${bin_dir}/gersemi" ]; then
        echo "${bin_dir}/gersemi"
    fi
}

# Sets clang_format, or exits with the command that installs it.
require_clang_format() {
    clang_format="$(find_clang_format)"
    if [ -z "${clang_format}" ]; then
        echo "format: clang-format was not found; run scripts/bootstrap.sh" >&2
        exit 1
    fi
    local major
    major="$("${clang_format}" --version | sed -n 's/.*clang-format version \([0-9]*\).*/\1/p')"
    if [ "${major}" != "${pinned_major}" ]; then
        echo "format: warning: ${clang_format} is version ${major}, not the pinned ${pinned_major};" \
            "its output may differ from CI" >&2
    fi
}

if [ "${print_clang_format}" -eq 1 ]; then
    require_clang_format
    echo "${clang_format}"
    exit 0
fi

status=0
if [ "${#cxx_files[@]}" -gt 0 ]; then
    require_clang_format
    if [ "${check}" -eq 1 ]; then
        "${clang_format}" --dry-run --Werror "${cxx_files[@]}" || status=1
    else
        "${clang_format}" -i "${cxx_files[@]}"
    fi
fi
if [ "${#cmake_files[@]}" -gt 0 ]; then
    gersemi="$(find_gersemi)"
    if [ -z "${gersemi}" ]; then
        echo "format: gersemi was not found; run scripts/bootstrap.sh" >&2
        exit 1
    fi
    if [ "${check}" -eq 1 ]; then
        "${gersemi}" --check "${cmake_files[@]}" || status=1
    else
        "${gersemi}" --in-place "${cmake_files[@]}"
    fi
fi
if [ "${status}" -ne 0 ]; then
    echo "format: files above are not formatted; run make format" >&2
fi
exit "${status}"
