#!/usr/bin/env bash
#
# update_sizes.sh — read build/sizes.csv and patch the size table into
# README.md, docs/building.md, and pages/guide/building.html.
#
# Usage:
#   scripts/update_sizes.sh            # print table to stdout
#   scripts/update_sizes.sh --update   # also patch the three doc files
#
# The table is delimited by sentinel comments:
#   <!-- SIZE_TABLE_START -->
#   ...
#   <!-- SIZE_TABLE_END -->
#
# Exit status: 0 on success, non-zero on missing CSV or extraction failure.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
cd "${PROJECT_ROOT}"

CSV="build/sizes.csv"
MODE="print"

for arg in "$@"; do
    case "$arg" in
        --update) MODE="update" ;;
        -h|--help)
            echo "Usage: scripts/update_sizes.sh [--update]"
            echo "  (no args)   Read build/sizes.csv, print size table"
            echo "  --update    Also patch README.md, docs/building.md, pages/guide/building.html"
            exit 0
            ;;
        *) echo "Unknown option: $arg" >&2; exit 1 ;;
    esac
done

if [ ! -f "${CSV}" ]; then
    echo "ERROR: ${CSV} not found. Run docker/build_sizes.sh first." >&2
    exit 1
fi

# -----------------------------------------------------------------------
# 1. Read CSV and sort by width then full_bytes ascending
# -----------------------------------------------------------------------

# Skip header, sort numerically by field 2 (width) then field 4 (full_bytes)
SORTED=$(tail -n +2 "${CSV}" | sort -t',' -k2,2n -k4,4n)

if [ -z "${SORTED}" ]; then
    echo "ERROR: No data rows in ${CSV}" >&2
    exit 1
fi

# Build markdown data rows
MD_ROWS=""
while IFS=',' read -r target width core full; do
    # Format bytes as X.X KB
    fmt_kb() {
        local val="$1"
        if [[ "${val}" =~ ^[0-9]+$ ]]; then
            awk "BEGIN { printf \"%.1f KB\", ${val}/1024.0 }"
        else
            echo "${val}"
        fi
    }
    row="| ${target} | $(fmt_kb "${core}") | $(fmt_kb "${full}") |"
    if [ -n "${MD_ROWS}" ]; then
        MD_ROWS+=$'\n'
    fi
    MD_ROWS+="${row}"
done <<< "${SORTED}"

# Build full markdown table
MD_TABLE="<!-- SIZE_TABLE_START -->"$'\n'
MD_TABLE+="| Target | Core | Full |"$'\n'
MD_TABLE+="|--------|-----:|-----:|"$'\n'
MD_TABLE+="${MD_ROWS}"$'\n'
MD_TABLE+="<!-- SIZE_TABLE_END -->"

echo "${MD_TABLE}"

if [ "${MODE}" != "update" ]; then
    exit 0
fi

# -----------------------------------------------------------------------
# 2. Patch markdown files
# -----------------------------------------------------------------------
patch_markdown() {
    local file="$1"
    if [ ! -f "$file" ]; then
        echo "  skip: $file not found" >&2
        return
    fi

    perl -0777 -i -pe "
        s{<!-- SIZE_TABLE_START -->.*?<!-- SIZE_TABLE_END -->}
         {${MD_TABLE}}s
    " "$file"

    echo "  patched: $file" >&2
}

patch_markdown "README.md"
patch_markdown "docs/building.md"

# -----------------------------------------------------------------------
# 3. Patch HTML file (pages/guide/building.html)
# -----------------------------------------------------------------------
patch_html() {
    local file="$1"
    if [ ! -f "$file" ]; then
        echo "  skip: $file not found" >&2
        return
    fi

    # Convert sorted CSV rows to HTML <tr> rows
    local html_rows=""
    while IFS=',' read -r target width core full; do
        fmt_kb() {
            local val="$1"
            if [[ "${val}" =~ ^[0-9]+$ ]]; then
                awk "BEGIN { printf \"%.1f KB\", ${val}/1024.0 }"
            else
                echo "${val}"
            fi
        }
        local tr="<tr><td>${target}</td><td>$(fmt_kb "${core}")</td><td>$(fmt_kb "${full}")</td></tr>"
        if [ -n "$html_rows" ]; then
            html_rows+=$'\n'
        fi
        html_rows+="${tr}"
    done <<< "${SORTED}"

    # Build the replacement block
    local replacement
    replacement="<!-- SIZE_TABLE_START -->"$'\n'
    replacement+="<table>"$'\n'
    replacement+="<thead><tr><th>Target</th><th>Core</th><th>Full</th></tr></thead>"$'\n'
    replacement+="<tbody>"$'\n'
    replacement+="${html_rows}"$'\n'
    replacement+="</tbody>"$'\n'
    replacement+="</table>"$'\n'
    replacement+="<!-- SIZE_TABLE_END -->"

    perl -0777 -i -pe "
        s{<!-- SIZE_TABLE_START -->.*?<!-- SIZE_TABLE_END -->}
         {${replacement}}s
    " "$file"

    echo "  patched: $file" >&2
}

patch_html "pages/guide/building.html"

echo "Size table updated in all doc files." >&2
