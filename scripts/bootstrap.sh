#!/usr/bin/env bash
# Installs the toolchain and tools, then clones vcpkg into .vcpkg/ at the pinned commit.
# Safe to run repeatedly; CI and the dev container run it too.
set -euo pipefail

# One pin per compiler and one for vcpkg. Keep VCPKG_COMMIT equal to builtin-baseline in vcpkg.json.
CLANG_VERSION="${CLANG_VERSION:-22}"
GCC_VERSION="${GCC_VERSION:-14}"
VCPKG_COMMIT="9e593bb18ea69cc5095e012465dcd675a822ed0d" # 2026.07.29

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

install_macos() {
    if ! command -v brew >/dev/null; then
        echo "bootstrap: Homebrew is required on macOS, see https://brew.sh" >&2
        exit 1
    fi
    for formula in llvm cmake ninja pkg-config; do
        brew list --versions "${formula}" >/dev/null || brew install "${formula}"
    done
}

install_linux() {
    if ! command -v apt-get >/dev/null; then
        echo "bootstrap: only apt-based Linux distributions are supported" >&2
        exit 1
    fi
    local sudo=""
    if [ "$(id -u)" -ne 0 ]; then sudo="sudo"; fi
    export DEBIAN_FRONTEND=noninteractive

    ${sudo} apt-get update -qq
    ${sudo} apt-get install -y -qq --no-install-recommends \
        ca-certificates cmake curl git make ninja-build pkg-config tar unzip zip \
        python3 python3-venv \
        "g++-${GCC_VERSION}"

    if ! apt-cache show "clang-${CLANG_VERSION}" >/dev/null 2>&1; then
        local codename
        codename="$(. /etc/os-release && echo "${VERSION_CODENAME}")"
        curl -fsSL https://apt.llvm.org/llvm-snapshot.gpg.key |
            ${sudo} tee /etc/apt/trusted.gpg.d/apt.llvm.org.asc >/dev/null
        echo "deb http://apt.llvm.org/${codename}/ llvm-toolchain-${codename}-${CLANG_VERSION} main" |
            ${sudo} tee "/etc/apt/sources.list.d/llvm-${CLANG_VERSION}.list" >/dev/null
        ${sudo} apt-get update -qq
    fi
    ${sudo} apt-get install -y -qq --no-install-recommends \
        "clang-${CLANG_VERSION}" "clang-tidy-${CLANG_VERSION}" "libclang-rt-${CLANG_VERSION}-dev"
}

install_vcpkg() {
    local dir="${root}/.vcpkg"
    if [ ! -d "${dir}/.git" ]; then
        git clone --quiet https://github.com/microsoft/vcpkg.git "${dir}"
    fi
    if [ "$(git -C "${dir}" rev-parse HEAD)" != "${VCPKG_COMMIT}" ]; then
        git -C "${dir}" cat-file -e "${VCPKG_COMMIT}^{commit}" 2>/dev/null ||
            git -C "${dir}" fetch --quiet origin
        git -C "${dir}" checkout --quiet --detach "${VCPKG_COMMIT}"
        rm -f "${dir}/vcpkg"
    fi
    if [ ! -x "${dir}/vcpkg" ]; then
        "${dir}/bootstrap-vcpkg.sh" -disableMetrics
    fi
}

case "$(uname -s)" in
    Darwin) install_macos ;;
    Linux) install_linux ;;
    *)
        echo "bootstrap: unsupported platform $(uname -s); use macOS or Linux" >&2
        exit 1
        ;;
esac
install_vcpkg
echo "bootstrap: done, next run: make dev"
