#!/usr/bin/env bash
#
# sync_version.sh — propagate the version from FR_MATH_VERSION_HEX in
# src/FR_math.h into every file that carries a user-visible version string.
# Idempotent: running it when everything already matches is a no-op.
#
# Single source of truth: src/FR_math.h
#   #define FR_MATH_VERSION_HEX  0xMMmmpp   (major << 16 | minor << 8 | patch)
#
# The hex define is what embedded targets return at runtime. Everything
# else — the string define, VERSION file, README badges, docs — is derived.
#
# To bump the version:
#   1. Edit FR_MATH_VERSION_HEX in src/FR_math.h
#   2. Run ./scripts/sync_version.sh
#
# Files kept in sync:
#   src/FR_math.h                — FR_MATH_VERSION string (derived from _HEX)
#   VERSION                      — plain-text "X.Y.Z" (derived from _HEX)
#   README.md                    — shields.io version badge
#   README.md                    — "Current version:" line
#   pages/assets/site.js         — FR_VERSION constant (docs page header)
#   src/FR_math_2D.h             — @version doxygen tag
#   src/FR_math_2D.cpp           — @version doxygen tag
#   library.properties           — Arduino Library Manager version
#   library.json                 — PlatformIO registry version
#   idf_component.yml            — ESP-IDF Component Registry version
#   llms.txt                     — AI coding agent reference version
#
# Manual review required at release time (no auto-sync):
#   llms.txt                     — verify API docs match any new/changed functions
#   agents.md                    — verify build/contribution instructions are current
#
# Usage:
#   ./scripts/sync_version.sh              # sync all files from FR_math.h
#   ./scripts/sync_version.sh --check      # verify only, exit 1 if any drift
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
        -h|--help)
            sed -n '3,34p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
            exit 0
            ;;
        *)
            echo -e "${RED}Unknown argument: ${arg}${NC}" >&2
            echo "Try $0 --help" >&2
            exit 2
            ;;
    esac
done

# --------------------------------------------------------------------------
# Source of truth: FR_MATH_VERSION_HEX in src/FR_math.h
# --------------------------------------------------------------------------
H_FILE="${PROJECT_ROOT}/src/FR_math.h"
if [[ ! -f "${H_FILE}" ]]; then
    echo -e "${RED}src/FR_math.h not found${NC}" >&2
    exit 2
fi

# Read the hex define and convert to X.Y.Z.
RAW_HEX=$(grep '#define FR_MATH_VERSION_HEX' "${H_FILE}" | awk '{print $3}' | tr -d '\r')
if [[ -z "${RAW_HEX}" ]]; then
    echo -e "${RED}FR_MATH_VERSION_HEX not found in src/FR_math.h${NC}" >&2
    exit 2
fi

# Convert 0xMMmmpp → decimal components.
HEX_NUM=$((RAW_HEX))
V_MAJ=$(( (HEX_NUM >> 16) & 0xff ))
V_MIN=$(( (HEX_NUM >> 8)  & 0xff ))
V_PAT=$(( HEX_NUM & 0xff ))
VERSION="${V_MAJ}.${V_MIN}.${V_PAT}"
WANT_HEX=$(printf "0x%02x%02x%02x" "${V_MAJ}" "${V_MIN}" "${V_PAT}")

echo -e "Source of truth: ${GREEN}FR_MATH_VERSION_HEX = ${WANT_HEX}${NC}  →  ${GREEN}${VERSION}${NC}"
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

# Helper for files with \r\n (FR_math.h): compare + sed instead of perl -i.
check_or_update_line() {
    local label="$1"
    local file="$2"
    local current="$3"
    local wanted="$4"
    local sed_expr="$5"

    if [[ "${current}" == "${wanted}" ]]; then
        echo -e "  ${GREEN}ok  ${NC} ${label}"
    elif [[ "${MODE}" == "check" ]]; then
        echo -e "  ${RED}DRIFT${NC} ${label}  (have '${current}', want '${wanted}')"
        DRIFT=1
    else
        sed "${sed_expr}" "${file}" > "${file}.tmp" && mv "${file}.tmp" "${file}"
        echo -e "  ${YELLOW}updated${NC} ${label}"
        CHANGED=1
    fi
}

# --------------------------------------------------------------------------
# 1. src/FR_math.h — FR_MATH_VERSION string (derived from _HEX)
# --------------------------------------------------------------------------
H_STR=$(grep '#define FR_MATH_VERSION ' "${H_FILE}" | grep '"' | sed 's/.*"\(.*\)".*/\1/' | tr -d '\r')
check_or_update_line "src/FR_math.h FR_MATH_VERSION" "${H_FILE}" \
    "${H_STR}" "${VERSION}" \
    "s/#define FR_MATH_VERSION .*\"[0-9]*\.[0-9]*\.[0-9]*\"/#define FR_MATH_VERSION     \"${VERSION}\"/"

# --------------------------------------------------------------------------
# 2. VERSION file (derived from _HEX)
# --------------------------------------------------------------------------
VER_FILE="${PROJECT_ROOT}/VERSION"
VER_CUR=""
if [[ -f "${VER_FILE}" ]]; then
    VER_CUR=$(head -1 "${VER_FILE}" | tr -d '[:space:]')
fi
if [[ "${VER_CUR}" == "${VERSION}" ]]; then
    echo -e "  ${GREEN}ok  ${NC} VERSION"
elif [[ "${MODE}" == "check" ]]; then
    echo -e "  ${RED}DRIFT${NC} VERSION  (have '${VER_CUR}', want '${VERSION}')"
    DRIFT=1
else
    echo "${VERSION}" > "${VER_FILE}"
    echo -e "  ${YELLOW}updated${NC} VERSION"
    CHANGED=1
fi

# --------------------------------------------------------------------------
# 3. README.md — shields.io version badge
#    Pattern: img.shields.io/badge/version-2.0.0-blue.svg
# --------------------------------------------------------------------------
update_file "README.md version badge" "${PROJECT_ROOT}/README.md" \
    "s|(img\\.shields\\.io/badge/version-)[0-9]+\\.[0-9]+\\.[0-9]+(-[a-z]+\\.svg)|\${1}${VERSION}\${2}|g"

# --------------------------------------------------------------------------
# 4. README.md — "Current version: X.Y.Z" line in the Version section
# --------------------------------------------------------------------------
update_file "README.md Current version: line" "${PROJECT_ROOT}/README.md" \
    "s|(Current version: )[0-9]+\\.[0-9]+\\.[0-9]+|\${1}${VERSION}|g"

# --------------------------------------------------------------------------
# 5. pages/assets/site.js — FR_VERSION constant
#    Pattern: var FR_VERSION = 'v2.0.0';
# --------------------------------------------------------------------------
update_file "pages/assets/site.js FR_VERSION" "${PROJECT_ROOT}/pages/assets/site.js" \
    "s|(var FR_VERSION = 'v)[0-9]+\\.[0-9]+\\.[0-9]+(';)|\${1}${VERSION}\${2}|g"

# --------------------------------------------------------------------------
# 6. src/FR_math_2D.h — @version doxygen tag
# --------------------------------------------------------------------------
update_file "src/FR_math_2D.h @version" "${PROJECT_ROOT}/src/FR_math_2D.h" \
    "s|(\\@version )[0-9]+\\.[0-9]+\\.[0-9]+|\${1}${VERSION}|g"

# --------------------------------------------------------------------------
# 7. src/FR_math_2D.cpp — @version doxygen tag
# --------------------------------------------------------------------------
update_file "src/FR_math_2D.cpp @version" "${PROJECT_ROOT}/src/FR_math_2D.cpp" \
    "s|(\\@version )[0-9]+\\.[0-9]+\\.[0-9]+|\${1}${VERSION}|g"

# --------------------------------------------------------------------------
# 8. library.properties — version field
#    Pattern: version=2.0.1
# --------------------------------------------------------------------------
update_file "library.properties version" "${PROJECT_ROOT}/library.properties" \
    "s|^(version=)[0-9]+\\.[0-9]+\\.[0-9]+|\${1}${VERSION}|"

# --------------------------------------------------------------------------
# 9. library.json — version field
#     Pattern: "version": "2.0.1"
# --------------------------------------------------------------------------
update_file "library.json version" "${PROJECT_ROOT}/library.json" \
    "s|(\"version\":\\s*\")[0-9]+\\.[0-9]+\\.[0-9]+(\")|\${1}${VERSION}\${2}|"

# --------------------------------------------------------------------------
# 10. idf_component.yml — version field
#     Pattern: version: "2.0.1"
# --------------------------------------------------------------------------
update_file "idf_component.yml version" "${PROJECT_ROOT}/idf_component.yml" \
    "s|(version:\\s*\")[0-9]+\\.[0-9]+\\.[0-9]+(\")|\${1}${VERSION}\${2}|"

# --------------------------------------------------------------------------
# 11. llms.txt — version line
#     Pattern: - Version: 2.0.1
# --------------------------------------------------------------------------
update_file "llms.txt version" "${PROJECT_ROOT}/llms.txt" \
    "s|(- Version: )[0-9]+\\.[0-9]+\\.[0-9]+|\${1}${VERSION}|"

# --------------------------------------------------------------------------
# Summary
# --------------------------------------------------------------------------
echo ""
if [[ "${MODE}" == "check" ]]; then
    if [[ "${DRIFT}" == "1" ]]; then
        echo -e "${RED}Version drift detected.${NC}"
        echo -e "${RED}Edit FR_MATH_VERSION_HEX in src/FR_math.h, then run ./scripts/sync_version.sh${NC}"
        exit 1
    fi
    echo -e "${GREEN}All files in sync with FR_MATH_VERSION_HEX=${WANT_HEX} (${VERSION}).${NC}"
else
    if [[ "${CHANGED}" == "1" ]]; then
        echo -e "${YELLOW}Version ${VERSION} (${WANT_HEX}) propagated. Review with 'git diff' and commit.${NC}"
    else
        echo -e "${GREEN}All files already at ${VERSION} (${WANT_HEX}). Nothing to do.${NC}"
    fi
fi
