#!/usr/bin/env bash
# Protects a repository created from the template: CI labels, a ruleset on main, merge settings, Dependabot.
# Prints one line per change it makes; a second run prints "no changes".
set -euo pipefail

program="${0##*/}"

usage() {
    cat <<USAGE
Usage: scripts/setup-repo.sh [--repo <owner>/<name>]

Sets up the GitHub repository of the current directory, or the one --repo names:
  - labels ci:full, ci:bench, and ci:msan, which CI reads to add jobs to a pull request;
  - a ruleset "main" on refs/heads/main that requires the status check and a linear history,
    plus the merge queue when the repository is public and an organization owns it;
  - squash as the only merge method, with the pull request title as the commit message;
  - Dependabot alerts.

Needs the GitHub CLI logged in as an administrator of the repository.
USAGE
}

fail() {
    echo "${program}: $*" >&2
    exit 1
}

repo=""
while [ "$#" -gt 0 ]; do
    case "$1" in
    -h | --help)
        usage
        exit 0
        ;;
    --repo)
        [ "$#" -ge 2 ] || fail "--repo needs <owner>/<name>"
        repo="$2"
        shift 2
        ;;
    *) fail "unknown argument $1; see scripts/setup-repo.sh --help" ;;
    esac
done

command -v gh >/dev/null || fail "the GitHub CLI is not installed; see https://cli.github.com"
gh auth status >/dev/null 2>&1 || fail "gh is not logged in to GitHub; run: gh auth login"
if [ -z "${repo}" ]; then
    repo="$(gh repo view --json nameWithOwner --jq .nameWithOwner)" ||
        fail "no GitHub repository here; pass --repo <owner>/<name>"
fi

changes=0
changed() {
    echo "${repo}: $*"
    changes=$((changes + 1))
}

# Labels.
existing_labels="$(gh label list --repo "${repo}" --limit 1000 --json name --jq '.[].name')"
while IFS='|' read -r label color description; do
    if ! grep -qxF "${label}" <<<"${existing_labels}"; then
        gh label create "${label}" --repo "${repo}" --color "${color}" --description "${description}" >/dev/null
        changed "created label ${label}"
    fi
done <<'LABELS'
ci:full|1d76db|Run every tier-two CI job, even on a draft
ci:bench|5319e7|Compare benchmarks against the base branch
ci:msan|b60205|Run the msan preset
LABELS

# Merge settings. Merge commits would break the linear history the ruleset requires.
merge_settings='{"allow_squash_merge":true,"allow_merge_commit":false,"allow_rebase_merge":false,
"squash_merge_commit_title":"PR_TITLE","squash_merge_commit_message":"BLANK"}'
merge_filter=". as \$repo | ${merge_settings} | all(to_entries[]; \$repo[.key] == .value)"
merge_current="$(gh api "repos/${repo}" --jq "${merge_filter}")"
if [ "${merge_current}" != true ]; then
    gh api --method PATCH "repos/${repo}" --input - <<<"${merge_settings}" >/dev/null
    changed "set squash as the only merge method, with the pull request title as the message"
fi

# Ruleset. GitHub offers the merge queue to public organization repositories, and to private ones only
# on Enterprise Cloud, which the API does not reveal; a private one gets none.
merge_queue=""
if [ "$(gh api "repos/${repo}" --jq '.owner.type + "/" + .visibility')" = Organization/public ]; then
    merge_queue=',{"type":"merge_queue","parameters":{"check_response_timeout_minutes":60,
"grouping_strategy":"ALLGREEN","max_entries_to_build":5,"max_entries_to_merge":5,"merge_method":"SQUASH",
"min_entries_to_merge":1,"min_entries_to_merge_wait_minutes":5}}'
else
    echo "${repo}: no merge queue, because GitHub offers it only to public organization repositories" >&2
fi
ruleset='{"name":"main","target":"branch","enforcement":"active",
"conditions":{"ref_name":{"include":["refs/heads/main"],"exclude":[]}},
"rules":[{"type":"required_status_checks","parameters":{"strict_required_status_checks_policy":false,
"do_not_enforce_on_create":false,"required_status_checks":[{"context":"status"}]}},
{"type":"required_linear_history"}'"${merge_queue}"']}'
ruleset_filter="{name: .name, target: .target, enforcement: .enforcement, conditions: .conditions, rules: .rules} == ${ruleset}"
ruleset_id="$(gh api "repos/${repo}/rulesets?includes_parents=false" --jq '.[] | select(.name == "main") | .id')"
ruleset_current=""
if [ -n "${ruleset_id}" ]; then
    ruleset_current="$(gh api "repos/${repo}/rulesets/${ruleset_id}" --jq "${ruleset_filter}")"
fi
if [ -z "${ruleset_id}" ]; then
    gh api --method POST "repos/${repo}/rulesets" --input - <<<"${ruleset}" >/dev/null ||
        fail "GitHub refused the ruleset; a private repository needs GitHub Pro, Team, or Enterprise for rulesets"
    changed "created ruleset main requiring the status check and a linear history${merge_queue:+, with the merge queue}"
elif [ "${ruleset_current}" != true ]; then
    gh api --method PUT "repos/${repo}/rulesets/${ruleset_id}" --input - <<<"${ruleset}" >/dev/null ||
        fail "GitHub refused to update ruleset main; check that you administer ${repo}"
    changed "reset ruleset main to require the status check and a linear history${merge_queue:+, with the merge queue}"
fi

# Dependabot alerts: the endpoint answers 204 when they are on and 404 when they are off.
if ! gh api --silent "repos/${repo}/vulnerability-alerts" 2>/dev/null; then
    gh api --silent --method PUT "repos/${repo}/vulnerability-alerts"
    # The endpoint keeps answering 404 for a few seconds, which would make a quick second run repeat this.
    for _ in $(seq 60); do
        if gh api --silent "repos/${repo}/vulnerability-alerts" 2>/dev/null; then break; fi
        sleep 1
    done
    changed "enabled Dependabot alerts"
fi

if [ "${changes}" -eq 0 ]; then echo "${repo}: no changes"; fi
