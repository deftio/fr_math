#!/usr/bin/env bash
#
# build.sh — one-shot clean rebuild of the FR_Math library, examples, and
# tests. Useful when switching branches, debugging stale object files, or
# for a quick sanity check after an edit.
#
# Usage:
#   ./scripts/build.sh            # clean + build lib, examples, tests, run tests
#   ./scripts/build.sh --no-test  # clean + build everything but skip running tests
#   ./scripts/build.sh --lib      # clean + build library only
#
# Exit status: 0 on success, non-zero if any build/test step fails.

set -euo pipefail

# Resolve project root (script lives in scripts/, project root is one level up).
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
cd "${PROJECT_ROOT}"

# Colors (disabled if stdout is not a TTY).
if [[ -t 1 ]]; then
    GREEN='\033[0;32m'
    YELLOW='\033[1;33m'
    RED='\033[0;31m'
    BOLD='\033[1m'
    NC='\033[0m'
else
    GREEN=''
    YELLOW=''
    RED=''
    BOLD=''
    NC=''
fi

# Parse args.
MODE="full"
for arg in "$@"; do
    case "$arg" in
        --no-test) MODE="no-test" ;;
        --lib)     MODE="lib-only" ;;
        -h|--help)
            sed -n '3,13p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
            exit 0
            ;;
        *)
            echo -e "${RED}Unknown argument: $arg${NC}" >&2
            echo "Try $0 --help" >&2
            exit 2
            ;;
    esac
done

echo ""
echo -e "${BOLD}=========================================${NC}"
echo -e "${BOLD}     FR_Math Clean Build${NC}"
echo -e "${BOLD}=========================================${NC}"
echo ""

# Step 1: clean.
echo -e "${YELLOW}[1/3] Cleaning build artifacts...${NC}"
bash "${SCRIPT_DIR}/clean_build.sh" >/dev/null
echo -e "${GREEN}  ok${NC}"

# Step 2: build library + examples.
echo -e "${YELLOW}[2/3] Building library and examples...${NC}"
make lib examples >/dev/null
echo -e "${GREEN}  ok${NC}"

if [[ "${MODE}" == "lib-only" ]]; then
    echo ""
    echo -e "${GREEN}Library + examples built successfully.${NC}"
    echo ""
    ls -lh build/*.o build/fr_example 2>/dev/null || true
    echo ""
    exit 0
fi

# Step 3: build tests and optionally run them.
if [[ "${MODE}" == "no-test" ]]; then
    echo -e "${YELLOW}[3/3] Building tests (not running)...${NC}"
    # `make test` also runs tests; to only build, depend on the test binaries.
    make build/fr_test build/test_comprehensive build/test_2d \
         build/test_overflow build/test_full build/test_2d_complete \
         build/test_tdd >/dev/null
    echo -e "${GREEN}  ok${NC}"
    echo ""
    echo -e "${GREEN}Build complete (tests not executed).${NC}"
    echo ""
    exit 0
fi

echo -e "${YELLOW}[3/3] Building and running tests...${NC}"
# Run make test but capture output so we can pretty-print a summary.
# `make test` prints per-suite results; we pipe through so the user sees it
# while also grepping the summary lines.
if ! make test 2>&1 | tee build/test.log; then
    echo ""
    echo -e "${RED}Build or tests FAILED. See output above.${NC}"
    exit 1
fi

echo ""
echo -e "${GREEN}=========================================${NC}"
echo -e "${GREEN}     Build + tests PASSED${NC}"
echo -e "${GREEN}=========================================${NC}"
echo ""
echo "Next steps:"
echo "  - ./scripts/coverage_report.sh    (coverage analysis)"
echo "  - ./scripts/size_report.sh        (object file sizes)"
echo "  - ./scripts/make_release.sh       (strict release validation)"
echo ""
