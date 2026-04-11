#!/usr/bin/env bash
#
# update_stats.sh — update test-count and coverage numbers across all
# docs, pages, and README badges after a full build+test.
#
# Usage:
#   ./scripts/update_stats.sh          # run tests + coverage, then update
#   ./scripts/update_stats.sh --dry    # show what would change, don't write
#
# Requires: make, lcov (for coverage), perl.
# Exit status: 0 on success, non-zero on failure.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
cd "${PROJECT_ROOT}"

DRY=0
for arg in "$@"; do
    case "$arg" in
        --dry) DRY=1 ;;
        -h|--help)
            sed -n '3,12p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
            exit 0
            ;;
        *) echo "Unknown argument: $arg" >&2; exit 2 ;;
    esac
done

# ── Step 1: build + test ──────────────────────────────────────────────

echo "Running make clean + make test ..."
make clean >/dev/null 2>&1
mkdir -p build
make test >build/make_test.log 2>&1
echo "  tests done."

# Count passing tests from suite summaries (e.g. "Passed: 15").
TESTS=$(grep -Eo "Passed: [0-9]+" build/make_test.log \
        | awk -F: '{sum += $2} END {print sum+0}')
echo "  total passing tests: ${TESTS}"

# ── Step 2: coverage ─────────────────────────────────────────────────

echo "Running coverage ..."
COV_LOG="/tmp/fr_math_cov_$$.log"
if bash "${SCRIPT_DIR}/coverage_report.sh" >"${COV_LOG}" 2>&1; then
    # Extract "Coverage: NN%" from the report (strip ANSI codes first).
    COV=$(sed 's/\x1b\[[0-9;]*m//g' "${COV_LOG}" \
          | grep -E "^  Coverage:" | grep -Eo "[0-9]+%" | tail -1)
    COV_NUM=${COV%\%}
else
    echo "  coverage_report.sh failed; using fallback via lcov"
    lcov --capture --directory build --output-file /tmp/coverage_$$.info \
         --quiet 2>/dev/null || true
    COV_NUM=$(lcov --summary /tmp/coverage_$$.info 2>&1 \
              | grep -Eo "[0-9]+\.[0-9]+%" | head -1 | cut -d. -f1)
    rm -f /tmp/coverage_$$.info
fi
rm -f "${COV_LOG}"
COV_NUM=${COV_NUM:-0}
echo "  coverage: ${COV_NUM}%"

# Badge color for shields.io.
if   [[ "${COV_NUM}" -ge 90 ]]; then COV_COLOR="brightgreen"
elif [[ "${COV_NUM}" -ge 80 ]]; then COV_COLOR="green"
elif [[ "${COV_NUM}" -ge 70 ]]; then COV_COLOR="yellowgreen"
elif [[ "${COV_NUM}" -ge 60 ]]; then COV_COLOR="yellow"
else                                  COV_COLOR="red"
fi

# ── Step 3: update files ─────────────────────────────────────────────

# All files that contain test count or coverage references.
FILES=(
    README.md
    docs/getting-started.md
    docs/releases.md
    docs/building.md
    pages/guide/getting-started.html
    pages/guide/building.html
    pages/releases.html
)

CHANGED=0

for f in "${FILES[@]}"; do
    fp="${PROJECT_ROOT}/${f}"
    [[ -f "${fp}" ]] || continue

    cp "${fp}" "${fp}.bak"

    # ── README badge URLs (shields.io) ──
    # Coverage: coverage-NN%25-color
    perl -i -pe "s|(img\\.shields\\.io/badge/coverage-)[0-9]+%25-[a-z]+|\${1}${COV_NUM}%25-${COV_COLOR}|g" "${fp}"
    # Tests: tests-NN%20passing-color
    perl -i -pe "s|(img\\.shields\\.io/badge/tests-)[0-9]+%20passing-[a-z]+|\${1}${TESTS}%20passing-brightgreen|g" "${fp}"

    # ── Prose: "NN tests" / "NN passing tests" ──
    # Match patterns like "40 tests", "40 passing tests"
    perl -i -pe "s/\b[0-9]+ tests\b/${TESTS} tests/g" "${fp}"
    perl -i -pe "s/\b[0-9]+ passing tests\b/${TESTS} passing tests/g" "${fp}"

    # ── Prose: "NN%" coverage (in context of "coverage") ──
    # "<strong>97%</strong> ... coverage" in HTML
    COV_NUM_EXPORT="${COV_NUM}" perl -i -pe 's|(<strong>)\d+%(</strong>)|$1$ENV{COV_NUM_EXPORT}%$2|g' "${fp}"
    # "**97%**" in markdown (bold percentage)
    COV_NUM_EXPORT="${COV_NUM}" perl -i -pe 's|(\*\*)\d+%(\*\*)|$1$ENV{COV_NUM_EXPORT}%$2|g' "${fp}"
    # "97% line coverage" or "97% line\n" (split across lines) in plain prose
    perl -i -pe "s/[0-9]+% line coverage/${COV_NUM}% line coverage/g" "${fp}"
    perl -i -pe "s/[0-9]+% line\$/${COV_NUM}% line/g" "${fp}"

    if cmp -s "${fp}" "${fp}.bak"; then
        rm -f "${fp}.bak"
    else
        CHANGED=$((CHANGED + 1))
        if [[ "${DRY}" == "1" ]]; then
            echo "  would update: ${f}"
            diff "${fp}.bak" "${fp}" || true
            mv "${fp}.bak" "${fp}"    # revert in dry mode
        else
            echo "  updated: ${f}"
            rm -f "${fp}.bak"
        fi
    fi
done

echo ""
if [[ "${DRY}" == "1" ]]; then
    echo "Dry run complete. ${CHANGED} file(s) would change."
else
    echo "Done. ${CHANGED} file(s) updated (tests=${TESTS}, coverage=${COV_NUM}%)."
fi
