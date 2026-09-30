#!/usr/bin/env bash
# Checks that a release tag names the version in project(VERSION): v<version>, or v<version>-<pre-release>.
# Prints the version and exits 0 when it does; .github/workflows/release.yaml runs it first.
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

if [ "$#" -ne 1 ]; then
    echo "Usage: scripts/check-release-tag.sh <tag>, for example v1.2.0 or v1.2.0-rc.1" >&2
    exit 2
fi
tag="$1"

version="$(sed -n 's/^project([^ ]* VERSION \([0-9.]*\).*/\1/p' "${root}/CMakeLists.txt")"
if [ -z "${version}" ]; then
    echo "check-release-tag: no project(<name> VERSION <x.y.z>) line in CMakeLists.txt" >&2
    exit 1
fi

# The tag's version is what follows v, up to a pre-release suffix.
tag_version="${tag#v}"
tag_version="${tag_version%%-*}"
if [[ ! "${tag}" =~ ^v[0-9.]+(-[0-9A-Za-z][0-9A-Za-z.-]*)?$ ]]; then
    echo "check-release-tag: tag ${tag} is not v<version> or v<version>-<pre-release>" >&2
    exit 1
fi
if [ "${tag_version}" != "${version}" ]; then
    echo "check-release-tag: tag ${tag} names version ${tag_version}, but project(VERSION) in CMakeLists.txt is ${version}" >&2
    exit 1
fi
echo "${version}"
