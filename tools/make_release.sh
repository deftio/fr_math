#!/usr/bin/env bash
#
# make_release.sh — Guided release pipeline for FR_Math.
#
# Walks through every step from local validation to published GitHub
# Release and package registry uploads, pausing for confirmation before
# anything visible to others.
#
# The FR_MATH_VERSION_HEX macro in src/FR_math.h is the single source
# of truth; the version is parsed directly from the hex define.
#
# Usage:
#   bash tools/make_release.sh                 # full guided release
#   bash tools/make_release.sh --validate      # local validation only
#   bash tools/make_release.sh --skip-cross    # skip cross-compile step
#
# Exit status: 0 if every step passes, non-zero on first failure.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
cd "${PROJECT_ROOT}"

# Log all output to build/release-<timestamp>.log
mkdir -p build
LOG_FILE="build/release-$(date -u '+%Y%m%d-%H%M%S').log"
exec > >(tee -a "$LOG_FILE") 2>&1
echo "Log: $LOG_FILE"

STEP=0
MODE="full"
BRANCH=""
ON_MASTER=false
TAG_EXISTS=false
SKIP_CROSS=0

# -----------------------------------------------------------------------
# Helpers
# -----------------------------------------------------------------------

step_header() {
    STEP=$((STEP + 1))
    echo ""
    echo "===[ Step $STEP: $1 ]==="
}

confirm() {
    local prompt="${1:-Continue?}"
    echo ""
    read -r -p "  $prompt [Y/n] " answer
    case "${answer:-y}" in
        [Yy]*) return 0 ;;
        *)
            echo "  Aborted by user."
            exit 1
            ;;
    esac
}

fail() {
    echo "  FAIL: $1" >&2
    exit 1
}

pass() {
    echo "  PASS: $1"
}

run_cmd() {
    echo "  \$ $*"
    "$@"
}

# -----------------------------------------------------------------------
# Step 1: Extract version from FR_MATH_VERSION_HEX
# -----------------------------------------------------------------------

do_extract_version() {
    step_header "Extract version from FR_math.h"

    local h_file="${PROJECT_ROOT}/src/FR_math.h"
    if [[ ! -f "${h_file}" ]]; then
        fail "src/FR_math.h not found"
    fi

    local raw_hex
    raw_hex=$(grep '#define FR_MATH_VERSION_HEX' "${h_file}" | awk '{print $3}' | tr -d '\r')
    if [[ -z "${raw_hex}" ]]; then
        fail "FR_MATH_VERSION_HEX not found in src/FR_math.h"
    fi

    local hex_num=$(( raw_hex ))
    VER_MAJ=$(( (hex_num >> 16) & 0xff ))
    VER_MIN=$(( (hex_num >> 8)  & 0xff ))
    VER_PAT=$(( hex_num & 0xff ))
    VER_STRING="${VER_MAJ}.${VER_MIN}.${VER_PAT}"
    VER_TAG="v${VER_STRING}"

    echo "  Version: $VER_STRING ($raw_hex)"
    echo "  Tag:     $VER_TAG"
}

# -----------------------------------------------------------------------
# Step 2: Sync version across all manifests
# -----------------------------------------------------------------------

do_sync_version() {
    step_header "Sync version across all manifests"

    if ! bash "${PROJECT_ROOT}/scripts/sync_version.sh" --check; then
        echo ""
        echo "  Running sync_version.sh to fix drift..."
        bash "${PROJECT_ROOT}/scripts/sync_version.sh"
        git add -A
        pass "Version synced to $VER_STRING (changes staged)"
    else
        pass "All version strings match $VER_STRING"
    fi
}

# -----------------------------------------------------------------------
# Step 3: Local validation (strict compile, tests, coverage)
# -----------------------------------------------------------------------

do_validate() {
    step_header "Local validation (build, tests, warnings, coverage)"

    echo "  --- Clean build ---"
    run_cmd make clean >/dev/null 2>&1

    echo ""
    echo "  --- Strict compile (-Wall -Wextra -Werror -Wshadow) ---"
    local strict_flags="-Isrc -Wall -Wextra -Werror -Wshadow -Os"
    mkdir -p build
    if ! cc ${strict_flags} -c src/FR_math.c -o build/FR_math_strict.o 2>build/strict_c.log; then
        cat build/strict_c.log
        fail "src/FR_math.c has compiler warnings"
    fi
    if ! c++ ${strict_flags} -c src/FR_math_2D.cpp -o build/FR_math_2D_strict.o 2>build/strict_cpp.log; then
        cat build/strict_cpp.log
        fail "src/FR_math_2D.cpp has compiler warnings"
    fi
    pass "Zero warnings."

    echo ""
    echo "  --- Build library + examples ---"
    run_cmd make lib examples >/dev/null 2>&1
    pass "Library and examples built."

    echo ""
    echo "  --- Full test suite ---"
    local test_log="build/make_test.log"
    if ! make test >"${test_log}" 2>&1; then
        tail -40 "${test_log}"
        fail "One or more test suites failed"
    fi
    if grep -E "Failed: [1-9]" "${test_log}" >/dev/null 2>&1; then
        grep -E "Failed: [1-9]" "${test_log}"
        fail "Test failures detected"
    fi
    TOTAL_PASSED=$(grep -Eo "Passed: [0-9]+" "${test_log}" | awk -F: '{sum+=$2} END {print sum}')
    pass "${TOTAL_PASSED} tests passed."

    echo ""
    echo "  --- Coverage ---"
    local cov_log="/tmp/fr_math_coverage_$$.log"
    if bash "${PROJECT_ROOT}/scripts/coverage_report.sh" >"${cov_log}" 2>&1; then
        OVERALL_PCT=$(sed 's/\x1b\[[0-9;]*m//g' "${cov_log}" | grep -E "^  Coverage:" | grep -Eo "[0-9]+%" | tail -1)
        pass "Coverage: ${OVERALL_PCT:-unknown}"
    else
        echo "  Coverage script failed (non-fatal)"
        OVERALL_PCT="unknown"
    fi
    rm -f "${cov_log}"

    echo ""
    echo "  --- Update README badges ---"
    local overall_num="${OVERALL_PCT%\%}"
    if [[ -n "${overall_num}" ]] && [[ "${overall_num}" =~ ^[0-9]+$ ]]; then
        local cov_color
        if   [[ "${overall_num}" -ge 90 ]]; then cov_color="brightgreen"
        elif [[ "${overall_num}" -ge 80 ]]; then cov_color="green"
        elif [[ "${overall_num}" -ge 70 ]]; then cov_color="yellowgreen"
        else                                      cov_color="yellow"
        fi
        perl -i -pe "s|(img\\.shields\\.io/badge/coverage-)[0-9]+%25-[a-z]+|\${1}${overall_num}%25-${cov_color}|g" README.md
        perl -i -pe "s|(img\\.shields\\.io/badge/tests-)[0-9]+%20passing-[a-z]+|\${1}${TOTAL_PASSED}%20passing-brightgreen|g" README.md
        pass "Badges updated (coverage=${overall_num}%, tests=${TOTAL_PASSED})"
    fi

    echo ""
    echo "  --- Size report ---"
    if command -v size >/dev/null 2>&1; then
        size build/FR_math.o build/FR_math_2D.o 2>/dev/null || ls -l build/FR_math.o build/FR_math_2D.o
    else
        ls -l build/FR_math.o build/FR_math_2D.o
    fi
}

# -----------------------------------------------------------------------
# Step 4: Cross-compile sanity check
# -----------------------------------------------------------------------

do_cross_compile() {
    step_header "Cross-compile sanity check"

    if [[ "${SKIP_CROSS}" == "1" ]]; then
        echo "  skipped (--skip-cross)"
        return 0
    fi

    local tried=0
    for cc_cmd in arm-none-eabi-gcc riscv64-unknown-elf-gcc arm-linux-gnueabi-gcc; do
        if command -v "${cc_cmd}" >/dev/null 2>&1; then
            tried=$((tried + 1))
            if "${cc_cmd}" -Isrc -Wall -Os -c src/FR_math.c -o /tmp/fr_math_${cc_cmd}.o 2>/dev/null; then
                pass "${cc_cmd}: ok"
            else
                fail "${cc_cmd} failed to compile FR_math.c"
            fi
            rm -f "/tmp/fr_math_${cc_cmd}.o"
        fi
    done
    if [[ "${tried}" == "0" ]]; then
        echo "  no cross toolchains installed; skipped"
    fi
}

# -----------------------------------------------------------------------
# Step 5: Commit pipeline-generated changes
# -----------------------------------------------------------------------

# Files the pipeline itself may modify (badge update, version sync).
# Anything outside this list is unexpected and should block the release.
PIPELINE_FILES="README.md VERSION src/FR_math.h library.properties library.json idf_component.yml llms.txt pages/assets/site.js src/FR_math_2D.h src/FR_math_2D.cpp"

do_commit_pipeline_changes() {
    step_header "Commit pipeline-generated changes"

    local status
    status=$(git status --porcelain)
    if [ -z "$status" ]; then
        pass "Working tree is clean — nothing to commit."
        return 0
    fi

    # Split dirty files into expected (pipeline) and unexpected.
    local unexpected=""
    local to_commit=""
    while IFS= read -r line; do
        # git status --porcelain: first two chars are status, then space, then path
        local file="${line:3}"
        local found=false
        for known in $PIPELINE_FILES; do
            if [ "$file" = "$known" ]; then
                found=true
                break
            fi
        done
        if $found; then
            to_commit="$to_commit $file"
        else
            unexpected="$unexpected $file"
        fi
    done <<< "$status"

    if [ -n "$unexpected" ]; then
        echo ""
        echo "  Unexpected uncommitted files:"
        for f in $unexpected; do
            echo "    $f"
        done
        echo ""
        fail "Commit or stash these files before running the release pipeline."
    fi

    echo "  Modified by pipeline:"
    for f in $to_commit; do
        echo "    $f"
    done

    confirm "Commit these files?"
    # shellcheck disable=SC2086
    git add $to_commit
    git commit -m "update badges and version strings for $VER_STRING"
    pass "Committed pipeline-generated changes."
}

# -----------------------------------------------------------------------
# Step 6: Check git state
# -----------------------------------------------------------------------

do_check_git() {
    step_header "Check git state"

    echo "  \$ git rev-parse --abbrev-ref HEAD"
    BRANCH=$(git rev-parse --abbrev-ref HEAD)
    echo "  Branch: $BRANCH"

    if [ "$BRANCH" = "master" ] || [ "$BRANCH" = "main" ]; then
        ON_MASTER=true
        echo "  Already on $BRANCH — will skip PR steps."
    fi

    # Tag check
    echo "  \$ git tag -l $VER_TAG"
    if git tag -l "$VER_TAG" | grep -q "$VER_TAG"; then
        echo "  Tag $VER_TAG already exists."
        if [ "$MODE" != "validate" ]; then
            local existing_release
            existing_release=$(gh release view "$VER_TAG" --json url --jq '.url' 2>/dev/null || true)
            if [ -n "$existing_release" ]; then
                echo ""
                pass "Release already published: $existing_release"
                echo "  Nothing to do. Bump FR_MATH_VERSION_HEX for the next release."
                exit 0
            fi
            echo "  Tag exists but no GitHub Release yet — will skip to release step."
            TAG_EXISTS=true
        else
            fail "Tag $VER_TAG already exists. Bump FR_MATH_VERSION_HEX in src/FR_math.h first."
        fi
    else
        pass "Tag $VER_TAG does not exist yet."
    fi

    # Working tree — should be clean after do_commit_pipeline_changes
    local status
    echo "  \$ git status --porcelain"
    status=$(git status --porcelain)
    if [ -n "$status" ]; then
        echo ""
        echo "  Uncommitted changes:"
        run_cmd git status --short
        fail "Working tree is dirty. Commit or stash before releasing."
    else
        pass "Working tree is clean."
    fi
}

# -----------------------------------------------------------------------
# Step 7: Sync with master and push branch
# -----------------------------------------------------------------------

do_push_branch() {
    if $ON_MASTER; then return 0; fi

    step_header "Sync with master and push branch '$BRANCH'"

    run_cmd git fetch origin master
    local behind
    behind=$(git rev-list --count "HEAD..origin/master" 2>/dev/null || echo "0")
    if [ "$behind" -gt 0 ]; then
        echo "  Branch is $behind commit(s) behind origin/master."
        echo "  Merging origin/master..."
        if ! run_cmd git merge origin/master --no-edit; then
            fail "Merge conflict. Resolve manually and re-run."
        fi
        pass "Merged origin/master into $BRANCH."
    else
        pass "Branch is up to date with master."
    fi

    local tracking
    tracking=$(git rev-parse --abbrev-ref --symbolic-full-name "@{u}" 2>/dev/null || true)
    if [ -n "$tracking" ]; then
        local ahead
        ahead=$(git rev-list --count "@{u}..HEAD" 2>/dev/null || echo "0")
        if [ "$ahead" -eq 0 ]; then
            pass "Branch is up to date with remote."
            return 0
        fi
        echo "  $ahead commit(s) ahead of remote."
    else
        echo "  No upstream tracking branch set."
    fi

    confirm "Push $BRANCH to origin?"
    run_cmd git push -u origin "$BRANCH"
    pass "Pushed."
}

# -----------------------------------------------------------------------
# Step 8: Open PR
# -----------------------------------------------------------------------

do_open_pr() {
    if $ON_MASTER; then return 0; fi

    step_header "Open PR to master"

    if ! command -v gh &>/dev/null; then
        fail "gh CLI not found. Install from https://cli.github.com/"
    fi

    local existing
    echo "  \$ gh pr list --head $BRANCH --base master --state open"
    existing=$(gh pr list --head "$BRANCH" --base master --state open --json number --jq '.[0].number' 2>/dev/null || true)
    if [ -n "$existing" ]; then
        echo "  PR #$existing already exists."
        PR_NUM="$existing"
        run_cmd gh pr view "$PR_NUM" --json title,url --jq '"  " + .title + "\n  " + .url'
        return 0
    fi

    echo "  No open PR found for $BRANCH -> master."
    confirm "Create PR?"

    local pr_url
    pr_url=$(gh pr create \
        --base master \
        --head "$BRANCH" \
        --title "Release $VER_STRING" \
        --body "Release $VER_STRING. See release_notes.md for details.")

    PR_NUM=$(gh pr view "$pr_url" --json number --jq '.number')
    echo "  Created: $pr_url"
}

# -----------------------------------------------------------------------
# Step 9: Wait for CI
# -----------------------------------------------------------------------

do_wait_ci() {
    if $ON_MASTER; then return 0; fi

    step_header "Wait for CI on PR #$PR_NUM"
    echo "  Polling every 30s..."
    echo ""

    local attempts=0
    local max_attempts=40
    while [ $attempts -lt $max_attempts ]; do
        local checks_json
        echo "  \$ gh pr checks $PR_NUM"
        checks_json=$(gh pr checks "$PR_NUM" --json name,state 2>/dev/null || true)

        if [ -z "$checks_json" ] || [ "$checks_json" = "[]" ]; then
            attempts=$((attempts + 1))
            echo "  Waiting for checks to start... (attempt $attempts/$max_attempts)"
            sleep 30
            continue
        fi

        echo "$checks_json" | python3 -c "
import sys, json
checks = json.load(sys.stdin)
for c in checks:
    icon = {'SUCCESS':'ok','FAILURE':'FAIL','PENDING':'...'}.get(c['state'], c['state'])
    print(f\"  [{icon:>4}] {c['name']}\")
" 2>/dev/null || echo "$checks_json"

        local any_pending any_failed
        any_pending=$(echo "$checks_json" | python3 -c "
import sys, json
checks = json.load(sys.stdin)
print('yes' if any(c['state'] == 'PENDING' for c in checks) else 'no')
" 2>/dev/null || echo "no")

        any_failed=$(echo "$checks_json" | python3 -c "
import sys, json
checks = json.load(sys.stdin)
print('yes' if any(c['state'] == 'FAILURE' for c in checks) else 'no')
" 2>/dev/null || echo "no")

        if [ "$any_failed" = "yes" ]; then
            echo ""
            fail "One or more CI checks failed. Fix the issue and re-run."
        fi

        if [ "$any_pending" = "no" ]; then
            echo ""
            pass "All CI checks passed."
            return 0
        fi

        attempts=$((attempts + 1))
        echo "  ... waiting 30s (attempt $attempts/$max_attempts)"
        echo ""
        sleep 30
    done

    echo ""
    echo "  Timed out waiting for CI after $max_attempts attempts."
    confirm "Continue without CI? (not recommended)"
}

# -----------------------------------------------------------------------
# Step 10: Merge PR
# -----------------------------------------------------------------------

do_merge_pr() {
    if $ON_MASTER; then return 0; fi

    step_header "Enable auto-merge on PR #$PR_NUM"

    local pr_state
    pr_state=$(gh pr view "$PR_NUM" --json state --jq '.state' 2>/dev/null || true)
    if [ "$pr_state" = "MERGED" ]; then
        pass "PR #$PR_NUM is already merged."
        return 0
    fi

    confirm "Enable auto-merge (squash) for PR #$PR_NUM?"
    run_cmd gh pr merge "$PR_NUM" --auto --squash --delete-branch
    pass "Auto-merge enabled. Will merge when all requirements are met."
}

# -----------------------------------------------------------------------
# Step 11: Wait for merge, switch to master
# -----------------------------------------------------------------------

do_switch_master() {
    if $ON_MASTER; then
        echo ""
        echo "  (Already on master, pulling latest)"
        run_cmd git pull --ff-only origin master
        return 0
    fi

    step_header "Wait for PR merge, then switch to master"
    echo "  Polling every 15s for merge completion..."

    local attempts=0
    while [ $attempts -lt 80 ]; do
        local pr_state
        echo "  \$ gh pr view $PR_NUM --json state"
        pr_state=$(gh pr view "$PR_NUM" --json state --jq '.state' 2>/dev/null || true)
        if [ "$pr_state" = "MERGED" ]; then
            echo ""
            pass "PR #$PR_NUM merged."
            break
        elif [ "$pr_state" = "CLOSED" ]; then
            fail "PR #$PR_NUM was closed without merging."
        fi
        attempts=$((attempts + 1))
        echo "  ... PR state: ${pr_state:-unknown} (attempt $attempts/80)"
        sleep 15
    done

    if [ $attempts -ge 80 ]; then
        fail "Timed out waiting for PR to merge."
    fi

    run_cmd git checkout master
    run_cmd git pull --ff-only origin master
    BRANCH="master"
    ON_MASTER=true
    pass "On master at $(git rev-parse --short HEAD)."
}

# -----------------------------------------------------------------------
# Step 12: Verify on master
# -----------------------------------------------------------------------

do_verify_master() {
    step_header "Verify build on master"

    run_cmd make clean >/dev/null 2>&1
    run_cmd make test >/dev/null 2>&1
    pass "All tests pass on master."
}

# -----------------------------------------------------------------------
# Step 13: Tag and push
# -----------------------------------------------------------------------

do_tag() {
    step_header "Create and push tag $VER_TAG"

    confirm "Create annotated tag $VER_TAG and push to origin?"
    run_cmd git tag -a "$VER_TAG" -m "FR_Math $VER_STRING"
    run_cmd git push origin "$VER_TAG"
    pass "Tag $VER_TAG pushed. Release workflow triggered."
}

# -----------------------------------------------------------------------
# Step 14: Wait for GitHub Release
# -----------------------------------------------------------------------

do_wait_release() {
    step_header "Wait for GitHub Release (created by release.yml)"
    echo "  Polling every 30s..."

    local attempts=0
    while [ $attempts -lt 40 ]; do
        local release_url
        echo "  \$ gh release view $VER_TAG"
        release_url=$(gh release view "$VER_TAG" --json url --jq '.url' 2>/dev/null || true)
        if [ -n "$release_url" ]; then
            echo ""
            pass "Release published!"
            echo "  $release_url"
            return 0
        fi

        # Check if Release workflow failed
        local run_conclusion
        run_conclusion=$(gh api repos/:owner/:repo/actions/runs \
            --jq ".workflow_runs[] | select(.name==\"Release\" and .head_branch==\"$VER_TAG\") | .conclusion" \
            2>/dev/null | head -1 || true)
        if [ "$run_conclusion" = "failure" ]; then
            echo ""
            echo "  Release workflow FAILED. Check:"
            echo "    https://github.com/deftio/fr_math/actions"
            echo ""
            echo "  To retry: delete the tag and re-run:"
            echo "    git tag -d $VER_TAG && git push origin :refs/tags/$VER_TAG"
            echo "  Or create the release manually:"
            echo "    gh release create $VER_TAG --title 'FR_Math $VER_STRING' --notes-file release_notes.md"
            exit 1
        fi

        attempts=$((attempts + 1))
        echo "  ... not yet (attempt $attempts/40)"
        sleep 30
    done

    echo ""
    echo "  Timed out waiting for release workflow."
    echo "  Check: https://github.com/deftio/fr_math/actions"
    echo "  Create release manually:"
    echo "    gh release create $VER_TAG --title 'FR_Math $VER_STRING' --notes-file release_notes.md"
    exit 1
}

# -----------------------------------------------------------------------
# Step 15: Publish to PlatformIO registry
# -----------------------------------------------------------------------

do_pio_publish() {
    step_header "Publish to PlatformIO registry"

    if ! command -v pio &>/dev/null; then
        echo "  pio CLI not found. Skipping."
        echo "  Install: pip install platformio"
        return 0
    fi

    # Check if already published at this version
    local pio_info
    pio_info=$(pio pkg show deftio/FR_Math 2>/dev/null || true)
    if echo "$pio_info" | grep -q "$VER_STRING"; then
        pass "FR_Math $VER_STRING already on PlatformIO registry."
        return 0
    fi

    confirm "Publish FR_Math $VER_STRING to PlatformIO?"
    run_cmd make clean >/dev/null 2>&1
    run_cmd pio pkg publish . --no-interactive
    pass "Published to PlatformIO registry."
}

# -----------------------------------------------------------------------
# Step 16: Publish to ESP-IDF Component Registry
# -----------------------------------------------------------------------

do_idf_publish() {
    step_header "Publish to ESP-IDF Component Registry"

    if ! command -v compote &>/dev/null; then
        echo "  compote CLI not found. Skipping."
        echo "  Install: pip install idf-component-manager"
        return 0
    fi

    confirm "Publish FR_Math $VER_STRING to ESP-IDF Component Registry?"
    echo "  --- Packing component ---"
    run_cmd compote component pack --name fr_math
    echo "  --- Uploading component ---"
    run_cmd compote component upload --name fr_math --namespace deftio
    pass "Published to ESP-IDF Component Registry."
}

# -----------------------------------------------------------------------
# Step 17: Done
# -----------------------------------------------------------------------

do_done() {
    echo ""
    echo "============================================"
    echo "  FR_Math $VER_STRING released successfully."
    echo "  Tag: $VER_TAG"
    local release_url
    release_url=$(gh release view "$VER_TAG" --json url --jq '.url' 2>/dev/null || true)
    if [ -n "$release_url" ]; then
        echo "  GitHub: $release_url"
    fi
    echo "============================================"
    echo ""
    echo "  Reminder: Arduino Library Manager indexes from GitHub tags."
    echo "  If FR_Math is registered, the new version will appear automatically."
    echo "  To register: submit a PR to https://github.com/arduino/library-registry"
}

# -----------------------------------------------------------------------
# Main
# -----------------------------------------------------------------------

PR_NUM=""
VER_STRING=""
VER_TAG=""
VER_MAJ=0
VER_MIN=0
VER_PAT=0
TOTAL_PASSED=0
OVERALL_PCT=""

case "${1:-}" in
    --validate)
        MODE="validate" ;;
    --skip-cross)
        SKIP_CROSS=1 ;;
    --help|-h)
        echo "Usage: bash tools/make_release.sh [--validate|--skip-cross]"
        echo ""
        echo "  (no args)        Full guided release (recommended)"
        echo "  --validate       Local validation only (build, tests, coverage)"
        echo "  --skip-cross     Skip cross-compile sanity check"
        echo ""
        exit 0
        ;;
    "")
        MODE="full" ;;
    *)
        echo "Unknown option: $1"
        echo "Run with --help for usage."
        exit 1
        ;;
esac

echo "============================================"
echo "  FR_Math release pipeline"
echo "  Mode: $MODE"
echo "============================================"

# -- Always run --
do_extract_version
do_sync_version
do_validate
do_cross_compile

if [ "$MODE" = "validate" ]; then
    echo ""
    echo "============================================"
    echo "  Validation passed."
    echo "  Version: $VER_STRING"
    echo "  Tests:   ${TOTAL_PASSED} passed"
    echo "  Coverage: ${OVERALL_PCT:-unknown}"
    echo "============================================"
    exit 0
fi

# -- Full release flow --
do_commit_pipeline_changes
do_check_git

if $TAG_EXISTS; then
    do_wait_release
    do_pio_publish
    do_idf_publish
    do_done
else
    do_push_branch
    do_open_pr
    do_wait_ci
    do_merge_pr
    do_switch_master
    do_verify_master
    do_tag
    do_wait_release
    do_pio_publish
    do_idf_publish
    do_done
fi
