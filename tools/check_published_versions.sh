#!/usr/bin/env bash
#
# check_published_versions.sh — show the published version of FR_Math
# on every distribution channel alongside the local source version.
#
# Usage:
#   bash tools/check_published_versions.sh
#
# Channels checked:
#   - Local source (FR_MATH_VERSION_HEX in src/FR_math.h)
#   - GitHub Releases (gh CLI or GitHub API)
#   - Arduino Library Manager (arduino-cli or library index API)
#   - PlatformIO Registry (pio CLI or registry API)
#   - ESP-IDF Component Registry (API only; compote has no query command)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

# Colors (disabled if stdout is not a TTY).
if [[ -t 1 ]]; then
    GREEN='\033[0;32m'
    YELLOW='\033[1;33m'
    RED='\033[0;31m'
    CYAN='\033[0;36m'
    NC='\033[0m'
else
    GREEN=''
    YELLOW=''
    RED=''
    CYAN=''
    NC=''
fi

# -----------------------------------------------------------------------
# Local source version
# -----------------------------------------------------------------------
LOCAL_VER="unknown"
H_FILE="${PROJECT_ROOT}/src/FR_math.h"
if [[ -f "${H_FILE}" ]]; then
    RAW_HEX=$(grep '#define FR_MATH_VERSION_HEX' "${H_FILE}" | awk '{print $3}' | tr -d '\r')
    if [[ -n "${RAW_HEX}" ]]; then
        HEX_NUM=$((RAW_HEX))
        V_MAJ=$(( (HEX_NUM >> 16) & 0xff ))
        V_MIN=$(( (HEX_NUM >> 8)  & 0xff ))
        V_PAT=$(( HEX_NUM & 0xff ))
        LOCAL_VER="${V_MAJ}.${V_MIN}.${V_PAT}"
    fi
fi

# -----------------------------------------------------------------------
# GitHub Releases
# -----------------------------------------------------------------------
get_github_version() {
    if command -v gh &>/dev/null; then
        gh api repos/deftio/fr_math/releases/latest --jq '.tag_name' 2>/dev/null | sed 's/^v//' || echo "not found"
    elif command -v curl &>/dev/null; then
        curl -sf https://api.github.com/repos/deftio/fr_math/releases/latest \
            | python3 -c "import sys,json; print(json.load(sys.stdin).get('tag_name','not found').lstrip('v'))" 2>/dev/null || echo "not found"
    else
        echo "no tool"
    fi
}

# -----------------------------------------------------------------------
# Arduino Library Manager
# -----------------------------------------------------------------------
get_arduino_version() {
    if command -v arduino-cli &>/dev/null; then
        local result
        result=$(arduino-cli lib search "FR_Math" 2>/dev/null || true)
        if echo "$result" | grep -q "FR_Math"; then
            echo "$result" | grep -A1 "^Name:" | grep "Version:" | awk '{print $2}' || echo "not found"
        else
            echo "not registered"
        fi
    elif command -v curl &>/dev/null; then
        local ver
        ver=$(curl -sf https://downloads.arduino.cc/libraries/library_index.json.gz 2>/dev/null \
            | gunzip 2>/dev/null \
            | python3 -c "
import sys, json
data = json.load(sys.stdin)
libs = [l for l in data['libraries'] if l['name'] == 'FR_Math']
if libs:
    latest = max(libs, key=lambda x: x['version'])
    print(latest['version'])
else:
    print('not registered')
" 2>/dev/null || echo "error")
        echo "$ver"
    else
        echo "no tool"
    fi
}

# -----------------------------------------------------------------------
# PlatformIO Registry
# -----------------------------------------------------------------------
get_platformio_version() {
    if command -v pio &>/dev/null; then
        pio pkg show deftio/FR_Math 2>/dev/null \
            | grep -i 'library' | grep -oE '[0-9]+\.[0-9]+\.[0-9]+' | head -1 || echo "not found"
    elif command -v curl &>/dev/null; then
        curl -sf "https://registry.platformio.org/api/packages/deftio/library/FR_Math" \
            | python3 -c "import sys,json; print(json.load(sys.stdin)['version']['name'])" 2>/dev/null || echo "not found"
    else
        echo "no tool"
    fi
}

# -----------------------------------------------------------------------
# ESP-IDF Component Registry
# -----------------------------------------------------------------------
get_espressif_version() {
    if command -v curl &>/dev/null; then
        curl -sf "https://components.espressif.com/api/components/deftio/fr_math" \
            | python3 -c "import sys,json; print(json.load(sys.stdin)['versions'][0]['version'])" 2>/dev/null || echo "not found"
    else
        echo "no tool"
    fi
}

# -----------------------------------------------------------------------
# Print results
# -----------------------------------------------------------------------
echo ""
echo -e "${CYAN}FR_Math Published Versions${NC}"
echo "=========================="
echo ""

BRANCH=$(git -C "${PROJECT_ROOT}" rev-parse --abbrev-ref HEAD 2>/dev/null || echo "unknown")
printf "  %-22s %s\n" "Branch:" "${BRANCH}"
printf "  %-22s %s\n" "Local version:" "${LOCAL_VER}"
echo ""

GITHUB_VER=$(get_github_version)
ARDUINO_VER=$(get_arduino_version)
PIO_VER=$(get_platformio_version)
ESP_VER=$(get_espressif_version)

color_version() {
    local ver="$1"
    if [[ "$ver" == "$LOCAL_VER" ]]; then
        echo -e "${GREEN}${ver}${NC}"
    elif [[ "$ver" == "not found" || "$ver" == "not registered" || "$ver" == "no tool" || "$ver" == "error" ]]; then
        echo -e "${YELLOW}${ver}${NC}"
    else
        echo -e "${RED}${ver}${NC} (drift)"
    fi
}

printf "  %-22s %b\n" "GitHub Releases:" "$(color_version "$GITHUB_VER")"
printf "  %-22s %b\n" "Arduino Library:" "$(color_version "$ARDUINO_VER")"
printf "  %-22s %b\n" "PlatformIO Registry:" "$(color_version "$PIO_VER")"
printf "  %-22s %b\n" "ESP-IDF Components:" "$(color_version "$ESP_VER")"
echo ""
