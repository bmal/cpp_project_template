#!/usr/bin/env bash
# Names the project: rewrites the current name in every lowercase, CamelCase, and UPPER_CASE form.
# The first run also leaves the template behind; a second run with the same arguments changes nothing.
set -euo pipefail

# shellcheck source-path=SCRIPTDIR source=lib/scaffold.sh
source "$(dirname "${BASH_SOURCE[0]}")/lib/scaffold.sh"

usage() {
    cat <<USAGE
Usage: scripts/init-project.sh <name> [--strip-samples]

Renames the project to <name>, lowercased with every run of other characters turned into _.
Run it again with another name to rename again.

The first run also sets project(VERSION) to 0.1.0, replaces README.md with a project stub,
and deletes .github/workflows/init.yaml, the workflow that runs this script on the first push.

--strip-samples removes the parser module and everything built on it: its unit tests,
the fuzz harness, its benchmark, core's echo_fields, and the integration test.
USAGE
}

name=""
strip=0
for arg in "$@"; do
    case "${arg}" in
    -h | --help)
        usage
        exit 0
        ;;
    --strip-samples) strip=1 ;;
    -*) fail "unknown option ${arg}; see scripts/init-project.sh --help" ;;
    *)
        if [ -n "${name}" ]; then fail "one name only, got '${name}' and '${arg}'"; fi
        name="${arg}"
        ;;
    esac
done
if [ -z "${name}" ]; then
    usage >&2
    exit 2
fi

cd "${root}"
self="scripts/init-project.sh"
init_workflow=".github/workflows/init.yaml"

# The current name: vcpkg.json holds the lowercase form with - for _, CMakeLists.txt the CamelCase one.
old_lower="$(sed -n 's/^  "name": "\([a-z0-9_-]*\)",$/\1/p' vcpkg.json | tr - _)"
old_camel="$(sed -n 's/^project(\([A-Za-z0-9]*\) VERSION .*/\1/p' CMakeLists.txt)"
old_upper="$(tr '[:lower:]' '[:upper:]' <<<"${old_lower}")"
if [ -z "${old_lower}" ] || [ -z "${old_camel}" ]; then
    fail "cannot read the current name from the name in vcpkg.json and project() in CMakeLists.txt"
fi

# The new name in every form: order_book, OrderBook, ORDER_BOOK, and order-book for vcpkg.
lower="$(tr '[:upper:]' '[:lower:]' <<<"${name}" | sed -E 's/[^a-z0-9]+/_/g; s/^_+//; s/_+$//')"
camel="$(camel_case "${lower}")"
upper="$(tr '[:lower:]' '[:upper:]' <<<"${lower}")"
hyphen="${lower//_/-}"

changed=0

# Every file git would commit, or every file when there is no repository; build trees excluded.
list_files() {
    if git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
        git ls-files -z --cached --others --exclude-standard | while IFS= read -r -d '' f; do
            if [ -f "${f}" ] && [ ! -L "${f}" ]; then printf '%s\0' "${f}"; fi
        done
    else
        find . -type f -not -path './.git/*' -not -path './build/*' -not -path './.vcpkg/*' -print0 |
            while IFS= read -r -d '' f; do printf '%s\0' "${f#./}"; done
    fi
}

# A rename replaces each form of the current name wherever it stands as a word part, so a new name
# must not already be one; otherwise a later rename would rewrite code that was never the project name.
# Markdown is exempt: rewriting prose breaks nothing, and the docs use example names.
# Keeping the current name skips the checks, since validate_name refuses it as a module name.
export self lower camel upper
if [ "${lower}" != "${old_lower}" ]; then
    validate_name "${lower}"
    # shellcheck disable=SC2016 # Perl expands $ENV{...} and $ARGV, not the shell.
    used="$(list_files | xargs -0 perl -ne 'next if $ARGV eq $ENV{self} || $ARGV =~ /\.md$/;
        if (/(?<![A-Za-z0-9])(\Q$ENV{lower}\E|\Q$ENV{camel}\E)(?![a-z0-9])|(?<![A-Za-z0-9])\Q$ENV{upper}\E(?![A-Z0-9])/) {
            print "$ARGV\n"; exit }')"
    if [ -n "${used}" ]; then fail "'${lower}' already appears in ${used}; pick a name the project does not use yet"; fi
fi

if [ "${old_lower}" != "${lower}" ] || [ "${old_camel}" != "${camel}" ]; then
    # A form matches only as a whole word part, so renaming a project called map spares bitmap.
    export old_lower old_camel old_upper
    # shellcheck disable=SC2016 # Perl expands $ENV{...}, not the shell.
    rewrite='s/(?<![A-Za-z0-9])\Q$ENV{old_lower}\E(?![a-z0-9])/$ENV{lower}/g;
        s/(?<![A-Za-z0-9])\Q$ENV{old_camel}\E(?![a-z0-9])/$ENV{camel}/g;
        s/(?<![A-Za-z0-9])\Q$ENV{old_upper}\E(?![A-Z0-9])/$ENV{upper}/g'
    count=0
    while IFS= read -r -d '' file; do
        if [ "${file}" = "${self}" ] || ! grep -qI -e "${old_lower}" -e "${old_camel}" -e "${old_upper}" "${file}"; then
            continue
        fi
        perl -pi -e "${rewrite}" "${file}"
        count=$((count + 1))
    done < <(list_files)
    # Paths: apps/<name>_cli and any other file or directory named after the project.
    while IFS= read -r -d '' file; do
        target="$(perl -pe "${rewrite}" <<<"${file}")"
        if [ "${target}" != "${file}" ]; then
            mkdir -p "$(dirname "${target}")"
            mv "${file}" "${target}"
            echo "moved ${file} to ${target}"
            rmdir -p "$(dirname "${file}")" 2>/dev/null || true
        fi
    done < <(list_files)
    sed -i.bak "s/^  \"name\": \"[a-z0-9_-]*\",$/  \"name\": \"${hyphen}\",/" vcpkg.json && rm vcpkg.json.bak
    echo "renamed ${old_lower}, ${old_camel}, and ${old_upper} to ${lower}, ${camel}, and ${upper} in ${count} files"
    changed=1
fi

# The first run on a copy of the template: the workflow that would run this script still exists.
if [ -e "${init_workflow}" ]; then
    sed -i.bak -E "s/^project\(${camel} VERSION [0-9.]+ /project(${camel} VERSION 0.1.0 /" CMakeLists.txt
    rm CMakeLists.txt.bak
    echo "set the version in CMakeLists.txt to 0.1.0"
    cat >README.md <<README
# ${camel}

Replace this line with what ${lower} does and who it is for.

## Quick Start

Install the toolchain, \`make\`, and vcpkg once per machine:

\`\`\`bash
scripts/bootstrap.sh
\`\`\`

Configure, build, and test:

\`\`\`bash
make dev
\`\`\`

List every other common action:

\`\`\`bash
make help
\`\`\`

## Documentation

| Page | Contents |
| --- | --- |
| [Getting started](docs/getting-started.md) | First build to first breakpoint |
| [How-to guides](docs/how-to/README.md) | One page per task |
| [Reference](docs/reference/README.md) | Presets, helper API, test labels, conventions |
| [Decisions](docs/decisions.md) | Why each choice was made and what was rejected |
README
    echo "replaced README.md with a project stub"
    rm "${init_workflow}"
    echo "deleted ${init_workflow}"
    changed=1
fi

if [ "${strip}" -eq 1 ] && [ -d libs/parser ]; then
    rm -rf libs/parser tests/unit/parser tests/fuzz benchmarks/parser tests/integration \
        libs/core/include/core/echo_fields.hpp libs/core/src/echo_fields.cpp \
        tests/support/include/support/line_builder.hpp
    echo "removed libs/parser, its tests, benchmark, and fuzz harness, core's echo_fields, and tests/integration"
    sed -i.bak "s/ PRIVATE_DEPS ${lower}::parser//" libs/core/CMakeLists.txt && rm libs/core/CMakeLists.txt.bak

    cat >"apps/${lower}_cli/main.cpp" <<CPP
// Mechanism: an app that wires modules together and keeps its own logic to orchestration.
// Prints the version banner; replace the body with what the program does.
#include <core/build_info.hpp>

#include <print>

// NOLINTNEXTLINE(bugprone-exception-escape): an escaping exception should terminate and dump core.
int main() {
    // std::print needs the Homebrew libc++ on macOS; the system copy lacks it.
    std::println("{}", ${lower}::core::version_banner());
    return 0;
}
CPP
    echo "reduced apps/${lower}_cli/main.cpp to the version banner"

    cat >tests/functional/test_cli.cpp <<CPP
// Functional tests of ${lower}_cli as a black box: stdin in, exit code and output out.
// They know the app only by its path and its documented behavior, never by its code.
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "cli_process.hpp"

namespace ${lower}::functional {

using testing::StartsWith;

TEST(Cli, PrintsTheBannerAndExitsZero) {
    const auto run = run_cli("");

    ASSERT_TRUE(run.has_value()) << run.error();
    EXPECT_EQ(run->exit_code, 0);
    EXPECT_THAT(run->out, StartsWith("${lower} "));
}

} // namespace ${lower}::functional
CPP
    echo "reduced tests/functional/test_cli.cpp to the banner"
    changed=1
fi

if [ "${changed}" -eq 0 ]; then
    echo "nothing to change: the project is already ${lower}"
fi
