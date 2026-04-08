#!/usr/bin/env bash
#
# make_release.sh — full release validation for FR_Math. Runs a strict
# clean build (warnings = errors), full test suite, coverage report, and
# size report. Leaves artifacts in build/ and coverage/ for inspection,
# and prints squash-merge instructions at the end.
#
# This script does NOT push, tag, or merge anything. It is a pre-merge
# validation gate — you run it locally, review the output, then follow
# the printed instructions to merge to master.
#
# Usage:
#   ./scripts/make_release.sh              # run full release validation
#   ./scripts/make_release.sh --skip-cross # skip cross-compile sanity (faster)
#
# Exit status: 0 if every step passes, non-zero on first failure.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
cd "${PROJECT_ROOT}"

# Colors (disabled if stdout is not a TTY).
if [[ -t 1 ]]; then
    GREEN='\033[0;32m'
    YELLOW='\033[1;33m'
    RED='\033[0;31m'
    BLUE='\033[0;34m'
    BOLD='\033[1m'
    NC='\033[0m'
else
    GREEN=''
    YELLOW=''
    RED=''
    BLUE=''
    BOLD=''
    NC=''
fi

SKIP_CROSS=0
for arg in "$@"; do
    case "$arg" in
        --skip-cross) SKIP_CROSS=1 ;;
        -h|--help)
            sed -n '3,18p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
            exit 0
            ;;
        *)
            echo -e "${RED}Unknown argument: $arg${NC}" >&2
            exit 2
            ;;
    esac
done

STEP=0
TOTAL_STEPS=9
step_header() {
    STEP=$((STEP + 1))
    echo ""
    echo -e "${BLUE}${BOLD}[${STEP}/${TOTAL_STEPS}] $1${NC}"
    echo -e "${BLUE}---------------------------------------------${NC}"
}

fail() {
    echo ""
    echo -e "${RED}${BOLD}RELEASE VALIDATION FAILED${NC}"
    echo -e "${RED}  Step: $1${NC}"
    echo -e "${RED}  Reason: $2${NC}"
    echo ""
    exit 1
}

# Record the current branch and any dirty state.
CURRENT_BRANCH=$(git rev-parse --abbrev-ref HEAD)
CURRENT_SHA=$(git rev-parse HEAD)
DIRTY=$(git status --porcelain)

echo ""
echo -e "${BOLD}=========================================${NC}"
echo -e "${BOLD}     FR_Math Release Validation${NC}"
echo -e "${BOLD}=========================================${NC}"
echo ""
echo -e "Branch:    ${CURRENT_BRANCH}"
echo -e "Commit:    ${CURRENT_SHA:0:12}"
if [[ -n "${DIRTY}" ]]; then
    echo -e "Status:    ${YELLOW}dirty (uncommitted changes)${NC}"
else
    echo -e "Status:    ${GREEN}clean${NC}"
fi

# ---------------------------------------------------------------------------
# Step 1: clean build tree.
# ---------------------------------------------------------------------------
step_header "Clean build tree"
bash "${SCRIPT_DIR}/clean_build.sh" >/dev/null
echo -e "${GREEN}  ok${NC}"

# ---------------------------------------------------------------------------
# Step 2: strict compile (warnings as errors).
# ---------------------------------------------------------------------------
step_header "Strict compile (-Wall -Wextra -Werror -Wshadow)"
STRICT_FLAGS="-Isrc -Wall -Wextra -Werror -Wshadow -Os"
mkdir -p build

# Library (C).
if ! cc ${STRICT_FLAGS} -c src/FR_math.c -o build/FR_math_strict.o 2>build/strict_c.log; then
    cat build/strict_c.log
    fail "strict compile" "src/FR_math.c has warnings (see above)"
fi
# Library (C++).
if ! c++ ${STRICT_FLAGS} -c src/FR_math_2D.cpp -o build/FR_math_2D_strict.o 2>build/strict_cpp.log; then
    cat build/strict_cpp.log
    fail "strict compile" "src/FR_math_2D.cpp has warnings (see above)"
fi
echo -e "${GREEN}  lib compiles clean with -Wall -Wextra -Werror -Wshadow${NC}"

# ---------------------------------------------------------------------------
# Step 3: standard build + full test suite.
# ---------------------------------------------------------------------------
step_header "Build library, examples, tests"
if ! make lib examples >build/make_lib.log 2>&1; then
    cat build/make_lib.log
    fail "make lib examples" "build failed"
fi
echo -e "${GREEN}  lib + examples built${NC}"

step_header "Run full test suite"
if ! make test >build/make_test.log 2>&1; then
    tail -40 build/make_test.log
    fail "make test" "one or more test suites failed"
fi
# Quick sanity: grep for "Failed:" lines and verify they all show 0 failures.
if grep -E "Failed: [1-9]" build/make_test.log >/dev/null; then
    grep -E "Failed: [1-9]" build/make_test.log
    fail "make test" "a test suite reported failures"
fi
TOTAL_PASSED=$(grep -Eo "Passed: [0-9]+" build/make_test.log | awk -F: '{sum+=$2} END {print sum}')
echo -e "${GREEN}  ${TOTAL_PASSED} tests passed across all suites${NC}"

# ---------------------------------------------------------------------------
# Step 4: coverage report.
# ---------------------------------------------------------------------------
step_header "Coverage report"
# coverage_report.sh wipes build/ at start, so log to /tmp.
COV_LOG="/tmp/fr_math_coverage_$$.log"
if ! bash "${SCRIPT_DIR}/coverage_report.sh" >"${COV_LOG}" 2>&1; then
    cat "${COV_LOG}"
    rm -f "${COV_LOG}"
    fail "coverage" "coverage_report.sh failed"
fi
# Extract the "Coverage: NN%" line from the overall summary (strip ANSI colors first).
OVERALL_PCT=$(sed 's/\x1b\[[0-9;]*m//g' "${COV_LOG}" | grep -E "^  Coverage:" | grep -Eo "[0-9]+%" | tail -1)
if [[ -z "${OVERALL_PCT}" ]]; then
    OVERALL_PCT="unknown"
fi
rm -f "${COV_LOG}"
echo -e "${GREEN}  overall coverage: ${OVERALL_PCT}${NC}"

# Warn if coverage drops below 90%.
OVERALL_NUM=${OVERALL_PCT%\%}
if [[ -n "${OVERALL_NUM}" ]] && [[ "${OVERALL_NUM}" =~ ^[0-9]+$ ]] && [[ "${OVERALL_NUM}" -lt 90 ]]; then
    echo -e "${YELLOW}  WARNING: coverage is below 90%${NC}"
fi

# ---------------------------------------------------------------------------
# Step 6: update README badges from actual numbers.
# ---------------------------------------------------------------------------
step_header "Update README badges"
README="${PROJECT_ROOT}/README.md"

if [[ ! -f "${README}" ]]; then
    echo -e "${YELLOW}  README.md not found; skipped${NC}"
elif [[ -z "${OVERALL_NUM}" ]] || ! [[ "${OVERALL_NUM}" =~ ^[0-9]+$ ]]; then
    echo -e "${YELLOW}  coverage % could not be parsed; skipped${NC}"
else
    # Pick a badge color based on coverage percentage (shields.io keywords).
    if   [[ "${OVERALL_NUM}" -ge 90 ]]; then COV_COLOR="brightgreen"
    elif [[ "${OVERALL_NUM}" -ge 80 ]]; then COV_COLOR="green"
    elif [[ "${OVERALL_NUM}" -ge 70 ]]; then COV_COLOR="yellowgreen"
    elif [[ "${OVERALL_NUM}" -ge 60 ]]; then COV_COLOR="yellow"
    else                                      COV_COLOR="red"
    fi

    # Snapshot README to detect whether badges actually changed.
    cp "${README}" "${README}.prev"

    # Rewrite the coverage badge URL:
    #   https://img.shields.io/badge/coverage-<N>%25-<color>.svg
    perl -i -pe "s|(img\\.shields\\.io/badge/coverage-)[0-9]+%25-[a-z]+|\${1}${OVERALL_NUM}%25-${COV_COLOR}|g" "${README}"

    # Rewrite the tests badge URL:
    #   https://img.shields.io/badge/tests-<N>%20passing-brightgreen.svg
    # Use 'brightgreen' whenever tests actually passed (they did, we're here).
    perl -i -pe "s|(img\\.shields\\.io/badge/tests-)[0-9]+%20passing-[a-z]+|\${1}${TOTAL_PASSED}%20passing-brightgreen|g" "${README}"

    if cmp -s "${README}" "${README}.prev"; then
        echo -e "${GREEN}  badges already up to date (coverage=${OVERALL_NUM}%, tests=${TOTAL_PASSED})${NC}"
    else
        echo -e "${GREEN}  updated: coverage=${OVERALL_NUM}% (${COV_COLOR}), tests=${TOTAL_PASSED} passing${NC}"
        echo -e "${YELLOW}  README.md was modified — remember to commit the badge change${NC}"
    fi
    rm -f "${README}.prev"
fi

# ---------------------------------------------------------------------------
# Step 7: size report.
# ---------------------------------------------------------------------------
step_header "Size report (current platform)"
if command -v size >/dev/null 2>&1; then
    size build/FR_math.o build/FR_math_2D.o 2>/dev/null || ls -l build/FR_math.o build/FR_math_2D.o
else
    ls -l build/FR_math.o build/FR_math_2D.o
fi
echo -e "${GREEN}  ok${NC}"

# ---------------------------------------------------------------------------
# Step 8: cross-compile sanity (optional, requires toolchains).
# ---------------------------------------------------------------------------
step_header "Cross-compile sanity check"
if [[ "${SKIP_CROSS}" == "1" ]]; then
    echo -e "${YELLOW}  skipped (--skip-cross)${NC}"
else
    CROSS_OK=0
    CROSS_TRIED=0
    for cc_cmd in arm-none-eabi-gcc riscv64-unknown-elf-gcc arm-linux-gnueabi-gcc; do
        if command -v "${cc_cmd}" >/dev/null 2>&1; then
            CROSS_TRIED=$((CROSS_TRIED + 1))
            # Only do a single-file -c compile check; we don't need a full link.
            if "${cc_cmd}" -Isrc -Wall -Os -c src/FR_math.c -o /tmp/fr_math_${cc_cmd}.o 2>/dev/null; then
                echo -e "${GREEN}  ${cc_cmd}: ok${NC}"
                CROSS_OK=$((CROSS_OK + 1))
            else
                echo -e "${RED}  ${cc_cmd}: FAILED${NC}"
                fail "cross-compile" "${cc_cmd} failed to compile FR_math.c"
            fi
            rm -f /tmp/fr_math_${cc_cmd}.o
        fi
    done
    if [[ "${CROSS_TRIED}" == "0" ]]; then
        echo -e "${YELLOW}  no cross toolchains installed; skipped${NC}"
    fi
fi

# ---------------------------------------------------------------------------
# Step 9: uncommitted changes check. Re-run git status because the badge
# step above may have modified README.md.
# ---------------------------------------------------------------------------
step_header "Working tree status"
DIRTY=$(git status --porcelain)
if [[ -n "${DIRTY}" ]]; then
    echo -e "${YELLOW}  Uncommitted changes present:${NC}"
    git status --short
    echo ""
    echo -e "${YELLOW}  You must commit (or stash) these before merging.${NC}"
else
    echo -e "${GREEN}  clean${NC}"
fi

# ---------------------------------------------------------------------------
# Final summary + squash-merge instructions.
# ---------------------------------------------------------------------------
echo ""
echo -e "${GREEN}${BOLD}=========================================${NC}"
echo -e "${GREEN}${BOLD}     RELEASE VALIDATION PASSED${NC}"
echo -e "${GREEN}${BOLD}=========================================${NC}"
echo ""
echo -e "Summary:"
echo -e "  Branch:        ${CURRENT_BRANCH}"
echo -e "  Commit:        ${CURRENT_SHA:0:12}"
echo -e "  Tests:         ${TOTAL_PASSED} passed"
echo -e "  Coverage:      ${OVERALL_PCT}"
echo ""
echo -e "${BOLD}Squash-merge checklist:${NC}"
echo ""
echo "  1. Commit any remaining changes on this branch:"
echo "       git add -A"
echo "       git commit -m \"your message\""
echo ""
echo "  2. Push the branch to origin:"
echo "       git push -u origin ${CURRENT_BRANCH}"
echo ""
echo "  3. Open a PR against master (via gh or the GitHub UI):"
echo "       gh pr create --base master --head ${CURRENT_BRANCH} \\"
echo "           --title \"<release title>\" --body-file release_notes.md"
echo ""
echo "  4. Wait for CI to pass on the PR."
echo ""
echo "  5. Squash-merge from the GitHub UI, or:"
echo "       gh pr merge --squash --delete-branch"
echo ""
echo "  6. After merge, tag the release from master:"
echo "       git checkout master && git pull"
echo "       git tag -a v2.0.0 -m \"FR_Math 2.0.0\""
echo "       git push origin v2.0.0"
echo ""
echo -e "${YELLOW}Note:${NC} this script does NOT push, tag, or merge for you."
echo ""
