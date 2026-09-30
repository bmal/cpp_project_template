#!/usr/bin/env bash
# Prints the CI image this checkout's Linux jobs run in: ghcr.io/<owner>/<repo>-ci:<inputs hash>.
# The tag hashes every file the image is built from, so a published tag always matches this tree.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

if [ "$#" -ne 0 ]; then
    echo "Usage: GITHUB_REPOSITORY=<owner>/<repo> scripts/ci-image.sh" >&2
    exit 2
fi
if [ -z "${GITHUB_REPOSITORY:-}" ]; then
    echo "ci-image: set GITHUB_REPOSITORY to <owner>/<repo>" >&2
    exit 2
fi

# The Dockerfile and the two files it copies; keep the list in step with its COPY lines.
inputs=("${root}/Dockerfile" "${root}/scripts/bootstrap.sh" "${root}/.pre-commit-config.yaml")
if command -v sha256sum >/dev/null; then
    hash="$(cat "${inputs[@]}" | sha256sum)"
else
    hash="$(cat "${inputs[@]}" | shasum -a 256)"
fi
echo "ghcr.io/$(echo "${GITHUB_REPOSITORY}" | tr '[:upper:]' '[:lower:]')-ci:${hash:0:16}"
