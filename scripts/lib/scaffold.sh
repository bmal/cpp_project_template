# shellcheck shell=bash
# Shared by new-module.sh and new-app.sh: name validation and file writing.
# Sourced, not executed; paths are relative to the repository root.

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
program="${0##*/}"

# C++23 keywords and alternative tokens; none of them can name a namespace.
cxx_keywords=" alignas alignof and and_eq asm auto bitand bitor bool break case catch char
char8_t char16_t char32_t class compl concept const consteval constexpr constinit const_cast
continue co_await co_return co_yield decltype default delete do double dynamic_cast else enum
explicit export extern false float for friend goto if inline int long mutable namespace new
noexcept not not_eq nullptr operator or or_eq private protected public register
reinterpret_cast requires return short signed sizeof static static_assert static_cast struct
switch template this thread_local throw true try typedef typeid typename union unsigned using
virtual void volatile wchar_t while xor xor_eq "

fail() {
    echo "${program}: $*" >&2
    exit 2
}

# Exits with a one-line reason unless $1 is a snake_case C++ identifier that is not a keyword.
validate_name() {
    local name="$1"
    if ! [[ "${name}" =~ ^[a-z][a-z0-9_]*$ ]]; then
        fail "'${name}' is not a snake_case C++ identifier; use lowercase letters, digits, and _, starting with a letter"
    fi
    if [[ "${name}" == *__* ]]; then
        fail "'${name}' contains __, which C++ reserves for the implementation"
    fi
    if [[ "$(tr '\n' ' ' <<<"${cxx_keywords}")" == *" ${name} "* ]]; then
        fail "'${name}' is a C++ keyword"
    fi
    if [ "${name}" = std ] || [ "${name}" = myproj ]; then
        fail "'${name}' would shadow namespace ${name} inside namespace myproj"
    fi
}

# Prints every CMake target name the project defines or CMake reserves, one per line.
taken_targets() {
    local dir
    printf '%s\n' all clean help install test package package_source edit_cache rebuild_cache \
        list_install_components myproj_warnings myproj_options myproj_test_support \
        myproj_bench_support run_benchmarks run_fuzz
    for dir in "${root}"/libs/*/; do
        dir="$(basename "${dir}")"
        printf '%s\n' "myproj_${dir}" "${dir}_unit_tests" "${dir}_benchmarks"
    done
    for dir in "${root}"/apps/*/; do basename "${dir}"; done
}

# Exits with a one-line reason if any of the given target names is already taken.
refuse_taken_targets() {
    local target taken
    taken="$(taken_targets)"
    for target in "$@"; do
        if grep -qxF "${target}" <<<"${taken}"; then
            fail "target ${target} already exists or is reserved by CMake; pick another name"
        fi
    done
}

# Exits with a one-line reason if any of the given paths under root exists.
refuse_existing() {
    local path
    for path in "$@"; do
        if [ -e "${root}/${path}" ]; then fail "${path} already exists; pick another name"; fi
    done
}

# Writes stdin to root/$1, creating its directory, and reports the path.
write_file() {
    mkdir -p "$(dirname "${root}/$1")"
    cat >"${root}/$1"
    echo "created $1"
}

# Prints $1 in CamelCase: price_level becomes PriceLevel.
camel_case() {
    awk -F_ '{ for (i = 1; i <= NF; i++) printf "%s%s", toupper(substr($i, 1, 1)), substr($i, 2) }' <<<"$1"
}
