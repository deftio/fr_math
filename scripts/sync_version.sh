#!/usr/bin/env bash
#
# sync_version.sh — propagate the contents of the repo-root VERSION file
# into every file that carries a user-visible version string. Idempotent:
# running it when everything already matches is a no-op.
#
# Single source of truth: /VERSION (one line, e.g. "2.0.0").
#
# Files kept in sync:
#   README.md                    — shields.io version badge
#   README.md                    — "Current version:" line in the Version section
#   pages/assets/site.js         — FR_VERSION constant (shown in every docs page header)
#   src/FR_math_2D.h             — @version doxygen tag
#   src/FR_math_2D.cpp           — @version doxygen tag
#   scripts/make_release.sh      — git tag hint in the squash-merge instructions
#
# Files NOT touched (history / descriptive prose):
#   release_notes.md             — canonical change log, user edits when bumping
#   pages/releases.html          — release history page
#   pages/**/*.html              — "As of v2.0.0" dated observations
#   docs/**/*.md                 — plain-text markdown mirror (human-maintained)
#
# Usage:
#   ./scripts/sync_version.sh              # update files to match VERSION
#   ./scripts/sync_version.sh --check      # verify only, exit 1 if any drift
#   ./scripts/sync_version.sh --set X.Y.Z  # rewrite VERSION first, then sync
#
# Exit status: 0 on success (everything in sync, or successfully synced).
#              1 on drift detected in --check mode.
#              2 on usage error.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
cd "${PROJECT_ROOT}"

# Colors (disabled if stdout is not a TTY).
if [[ -t 1 ]]; then
    GREEN='\033[0;32m'
    YELLOW='\033[1;33m'
    RED='\033[0;31m'
    NC='\033[0m'
else
    GREEN=''
    YELLOW=''
    RED=''
    NC=''
fi

MODE="sync"
for arg in "$@"; do
    case "$arg" in
        --check)
            MODE="check"
            ;;
        --set)
            echo -e "${RED}--set requires a value: --set X.Y.Z${NC}" >&2
            exit 2
            ;;
        --set=*)
            NEW_VER="${arg#--set=}"
            echo "${NEW_VER}" > "${PROJECT_ROOT}/VERSION"
            ;;
        -h|--help)
            sed -n '3,32p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
            exit 0
            ;;
        *)
            # Positional: treat as the new version if it looks like X.Y.Z.
            if [[ "${arg}" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
                echo "${arg}" > "${PROJECT_ROOT}/VERSION"
            else
                echo -e "${RED}Unknown argument: ${arg}${NC}" >&2
                echo "Try $0 --help" >&2
                exit 2
            fi
            ;;
    esac
done

# Read source of truth.
if [[ ! -f "${PROJECT_ROOT}/VERSION" ]]; then
    echo -e "${RED}VERSION file not found at ${PROJECT_ROOT}/VERSION${NC}" >&2
    exit 2
fi
VERSION=$(head -1 "${PROJECT_ROOT}/VERSION" | tr -d '[:space:]')
if [[ ! "${VERSION}" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
    echo -e "${RED}VERSION file does not contain a valid X.Y.Z version: '${VERSION}'${NC}" >&2
    exit 2
fi

echo -e "Target version: ${GREEN}${VERSION}${NC}"
echo ""

# Track whether we made any modifications (for drift detection in --check mode).
DRIFT=0
CHANGED=0

# --------------------------------------------------------------------------
# Helper: apply a perl -i substitution, snapshot-diff to detect real change.
# Args: label, file, perl_expression
# --------------------------------------------------------------------------
update_file() {
    local label="$1"
    local file="$2"
    local perl_expr="$3"

    if [[ ! -f "${file}" ]]; then
        echo -e "  ${YELLOW}skip${NC} ${label} (${file} not found)"
        return
    fi

    cp "${file}" "${file}.prev"
    perl -i -pe "${perl_expr}" "${file}"

    if cmp -s "${file}" "${file}.prev"; then
        echo -e "  ${GREEN}ok  ${NC} ${label}"
        rm -f "${file}.prev"
    else
        if [[ "${MODE}" == "check" ]]; then
            # Restore and flag drift.
            mv "${file}.prev" "${file}"
            echo -e "  ${RED}DRIFT${NC} ${label}"
            DRIFT=1
        else
            echo -e "  ${YELLOW}updated${NC} ${label}"
            rm -f "${file}.prev"
            CHANGED=1
        fi
    fi
}

# --------------------------------------------------------------------------
# 1. README.md — shields.io version badge
#    Pattern: img.shields.io/badge/version-2.0.0-blue.svg
# --------------------------------------------------------------------------
update_file "README.md version badge" "${PROJECT_ROOT}/README.md" \
    "s|(img\\.shields\\.io/badge/version-)[0-9]+\\.[0-9]+\\.[0-9]+(-[a-z]+\\.svg)|\${1}${VERSION}\${2}|g"

# --------------------------------------------------------------------------
# 2. README.md — "Current version: X.Y.Z" line in the Version section
# --------------------------------------------------------------------------
update_file "README.md Current version: line" "${PROJECT_ROOT}/README.md" \
    "s|(Current version: )[0-9]+\\.[0-9]+\\.[0-9]+|\${1}${VERSION}|g"

# --------------------------------------------------------------------------
# 3. pages/assets/site.js — FR_VERSION constant
#    Pattern: var FR_VERSION = 'v2.0.0';
# --------------------------------------------------------------------------
update_file "pages/assets/site.js FR_VERSION" "${PROJECT_ROOT}/pages/assets/site.js" \
    "s|(var FR_VERSION = 'v)[0-9]+\\.[0-9]+\\.[0-9]+(';)|\${1}${VERSION}\${2}|g"

# --------------------------------------------------------------------------
# 4. src/FR_math_2D.h — @version doxygen tag
# --------------------------------------------------------------------------
update_file "src/FR_math_2D.h @version" "${PROJECT_ROOT}/src/FR_math_2D.h" \
    "s|(\\@version )[0-9]+\\.[0-9]+\\.[0-9]+|\${1}${VERSION}|g"

# --------------------------------------------------------------------------
# 5. src/FR_math_2D.cpp — @version doxygen tag
# --------------------------------------------------------------------------
update_file "src/FR_math_2D.cpp @version" "${PROJECT_ROOT}/src/FR_math_2D.cpp" \
    "s|(\\@version )[0-9]+\\.[0-9]+\\.[0-9]+|\${1}${VERSION}|g"

# --------------------------------------------------------------------------
# 6. scripts/make_release.sh — git tag hint in squash-merge instructions
#    Patterns:
#       git tag -a v2.0.0 -m "FR_Math 2.0.0"
#       git push origin v2.0.0
# --------------------------------------------------------------------------
update_file "scripts/make_release.sh tag hint" "${PROJECT_ROOT}/scripts/make_release.sh" \
    "s|v[0-9]+\\.[0-9]+\\.[0-9]+|v${VERSION}|g if /git (tag|push origin v)/"
# The perl above only substitutes on lines that match the git tag/push hint,
# so we don't accidentally clobber other version strings elsewhere in the
# script.
update_file "scripts/make_release.sh tag message" "${PROJECT_ROOT}/scripts/make_release.sh" \
    "s|(FR_Math )[0-9]+\\.[0-9]+\\.[0-9]+|\${1}${VERSION}|g"

# --------------------------------------------------------------------------
# Summary
# --------------------------------------------------------------------------
echo ""
if [[ "${MODE}" == "check" ]]; then
    if [[ "${DRIFT}" == "1" ]]; then
        echo -e "${RED}Version drift detected. Run ./scripts/sync_version.sh to fix.${NC}"
        exit 1
    fi
    echo -e "${GREEN}All files in sync with VERSION=${VERSION}.${NC}"
else
    if [[ "${CHANGED}" == "1" ]]; then
        echo -e "${YELLOW}Version ${VERSION} propagated. Review with 'git diff' and commit.${NC}"
    else
        echo -e "${GREEN}All files already at ${VERSION}. Nothing to do.${NC}"
    fi
fi
