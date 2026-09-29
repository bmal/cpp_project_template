#!/usr/bin/env bash
# Builds libc++ and libc++abi with MemorySanitizer for the project's Clang, once, for the msan preset.
# Safe to run repeatedly: a finished build is reused, and the script prints where it is.
set -euo pipefail

usage() {
    cat <<EOF
Usage: scripts/build-msan-libcxx.sh [--check]

Builds an instrumented libc++ into \${XDG_CACHE_HOME:-~/.cache}/myproj/msan-libcxx/<clang version>/
for the Clang the msan preset uses, then prints that directory. Linux only.

  --check  Exit 0 when the build exists, 1 when it does not; build nothing.
EOF
}

check_only=0
case "${1:-}" in
"") ;;
--check) check_only=1 ;;
-h | --help)
    usage
    exit 0
    ;;
*)
    usage >&2
    exit 2
    ;;
esac

if [ "$(uname -s)" != Linux ]; then
    echo "build-msan-libcxx: MemorySanitizer runs on Linux only" >&2
    exit 1
fi

# The same search as cmake/toolchain/compiler.cmake: the newest versioned clang++, then clang++.
cxx=""
for version in $(seq 30 -1 14) ""; do
    if command -v "clang++${version:+-${version}}" >/dev/null; then
        cxx="$(command -v "clang++${version:+-${version}}")"
        break
    fi
done
if [ -z "${cxx}" ]; then
    echo "build-msan-libcxx: no clang++ was found; run scripts/bootstrap.sh" >&2
    exit 1
fi
cc="${cxx%++*}${cxx##*++}"
full_version="$("${cxx}" --version | sed -n 's/.*clang version \([0-9][0-9]*\.[0-9][0-9]*\.[0-9][0-9]*\).*/\1/p')"
major="${full_version%%.*}"

# cmake/toolchain/compiler.cmake computes the same directory; keep the two in step.
prefix="${XDG_CACHE_HOME:-${HOME}/.cache}/myproj/msan-libcxx/${full_version}"
if [ -e "${prefix}/lib/libc++.so" ]; then
    echo "build-msan-libcxx: already built for Clang ${full_version} in ${prefix}"
    exit 0
fi
if [ "${check_only}" -eq 1 ]; then
    echo "build-msan-libcxx: not built for Clang ${full_version}; expected ${prefix}" >&2
    exit 1
fi

work="$(mktemp -d "${TMPDIR:-/tmp}/msan-libcxx.XXXXXX")"
trap 'rm -rf "${work}"' EXIT

# Sources of the matching release tag, or its release branch when the tag does not exist yet.
echo "build-msan-libcxx: fetching LLVM ${full_version} sources"
ref="llvmorg-${full_version}"
if ! git ls-remote --exit-code --tags https://github.com/llvm/llvm-project "${ref}" >/dev/null; then
    ref="release/${major}.x"
fi
git clone --quiet --depth 1 --branch "${ref}" --filter=blob:none --sparse \
    https://github.com/llvm/llvm-project "${work}/llvm-project"
git -C "${work}/llvm-project" sparse-checkout set \
    runtimes cmake libc libcxx libcxxabi libunwind llvm/cmake llvm/utils/llvm-lit third-party

echo "build-msan-libcxx: building with ${cxx}, a few minutes"
cmake -S "${work}/llvm-project/runtimes" -B "${work}/build" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER="${cc}" \
    -DCMAKE_CXX_COMPILER="${cxx}" \
    -DCMAKE_INSTALL_PREFIX="${prefix}.partial" \
    -DLLVM_ENABLE_RUNTIMES="libcxx;libcxxabi" \
    -DLLVM_USE_SANITIZER=MemoryWithOrigins \
    -DLLVM_ENABLE_PER_TARGET_RUNTIME_DIR=OFF \
    -DLIBCXXABI_USE_LLVM_UNWINDER=OFF \
    -DLIBCXX_INCLUDE_TESTS=OFF \
    -DLIBCXX_INCLUDE_BENCHMARKS=OFF \
    -DLIBCXXABI_INCLUDE_TESTS=OFF
cmake --build "${work}/build" --target install-cxx install-cxxabi
# Moved into place only when complete, so an interrupted build is never mistaken for a finished one.
rm -rf "${prefix}"
mv "${prefix}.partial" "${prefix}"
echo "build-msan-libcxx: built for Clang ${full_version} in ${prefix}"
