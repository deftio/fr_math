#!/usr/bin/env bash
#
# clean_build.sh — wipe the build/ and coverage/ directories and recreate
# the build directory so the next `make` works without error.
#
# Why this exists: `make clean` removes build/ entirely, but several targets
# (like `make test-tdd`) call the compiler with an explicit `-o build/foo.o`
# path. If build/ doesn't exist, the compiler fails with
# "unable to open output file". This script wipes everything and re-creates
# the empty build directory in one step.
#
# Usage:
#   ./scripts/clean_build.sh           # clean and recreate build/
#   ./scripts/clean_build.sh --all     # also remove tilde backup files
#
# Exit status: 0 on success, non-zero if rm/mkdir fails.

set -euo pipefail

# Resolve the project root (script lives in scripts/, project is one level up).
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

cd "${PROJECT_ROOT}"

echo "Cleaning build artifacts in ${PROJECT_ROOT}"

# Remove build and coverage trees.
rm -rf build coverage

# Remove stray gcov files in the source tree (in case anyone ran gcov from there).
find . -name '*.gcda' -delete 2>/dev/null || true
find . -name '*.gcno' -delete 2>/dev/null || true
find . -name '*.gcov' -delete 2>/dev/null || true

# Remove top-level *.o, *.exe, *.info files (matches `make clean`).
rm -f ./*.o ./*.exe ./*.info

# Optional: also remove editor backup files.
if [[ "${1:-}" == "--all" ]]; then
    echo "  --all: removing ~ backup files"
    find . -name '*~' -delete 2>/dev/null || true
fi

# Recreate empty build directory so subsequent `make` invocations succeed
# without needing the makefile to mkdir on every target.
mkdir -p build coverage

echo "Done. build/ and coverage/ recreated empty."
