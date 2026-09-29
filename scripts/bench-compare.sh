#!/usr/bin/env bash
# Compares the benchmarks of the working tree against a git ref with Google Benchmark's compare.py.
# The ref is built with the same bench preset in a git worktree under build/bench-base/.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
worktree="${root}/build/bench-base"
venv="${root}/build/bench-venv"

if [ "$#" -ne 1 ] || [ "$1" = -h ] || [ "$1" = --help ]; then
    echo "Usage: scripts/bench-compare.sh <ref>, for example main or HEAD~3" >&2
    exit 2
fi
fail() {
    echo "bench-compare: $*" >&2
    exit 1
}

commit="$(git -C "${root}" rev-parse --verify --quiet "$1^{commit}")" || fail "'$1' is not a commit"
if [ -z "${VCPKG_ROOT:-}" ] && [ -d "${root}/.vcpkg" ]; then
    export VCPKG_ROOT="${root}/.vcpkg"
fi

# Configures, builds, and runs every benchmark of the tree at $1, writing build/bench/bench/*.json.
# Old results go first, so a benchmark that no longer exists is never compared.
# Chained with &&, because set -e does not apply inside a function called with ||.
run_benchmarks() {
    rm -rf "$1/build/bench/bench" &&
        cmake -S "$1" --preset bench >/dev/null &&
        cmake --build "$1/build/bench" --target run_benchmarks
}

git -C "${root}" worktree prune
if [ -e "${worktree}/.git" ]; then
    git -C "${worktree}" checkout --quiet --detach "${commit}"
else
    rm -rf "${worktree}"
    git -C "${root}" worktree add --quiet --detach "${worktree}" "${commit}"
fi

echo "bench-compare: running the baseline, $1 at ${commit:0:12}"
run_benchmarks "${worktree}" || fail "the baseline failed to build; does $1 have the bench preset?"
echo "bench-compare: running the working tree"
run_benchmarks "${root}"

# compare.py ships with the benchmark port and needs numpy and scipy, kept in a local venv.
if [ ! -x "${venv}/bin/python" ]; then
    python3 -m venv "${venv}"
    "${venv}/bin/pip" install --quiet numpy scipy
fi
compare=("${root}"/build/bench/vcpkg_installed/*/tools/benchmark/compare.py)
[ -f "${compare[0]}" ] || fail "compare.py was not found under build/bench/vcpkg_installed/"

for contender in "${root}"/build/bench/bench/*.json; do
    baseline="${worktree}/build/bench/bench/$(basename "${contender}")"
    if [ ! -f "${baseline}" ]; then
        echo "bench-compare: $(basename "${contender}" .json) is new, nothing to compare"
        continue
    fi
    echo
    echo "== $(basename "${contender}" .json): $1 -> working tree"
    "${venv}/bin/python" "${compare[0]}" --no-color --display_aggregates_only \
        benchmarks "${baseline}" "${contender}"
done
