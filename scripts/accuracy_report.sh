#!/usr/bin/env bash
#
# accuracy_report.sh — extract the accuracy table from test_tdd and
# optionally patch it into README.md, docs/README.md, and pages/index.html.
#
# Usage:
#   scripts/accuracy_report.sh            # build, run, print table to stdout
#   scripts/accuracy_report.sh --update   # also patch the three doc files
#
# The table is delimited by sentinel comments:
#   <!-- ACCURACY_TABLE_START -->
#   ...
#   <!-- ACCURACY_TABLE_END -->
#
# Exit status: 0 on success, non-zero on build or extraction failure.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
cd "${PROJECT_ROOT}"

MODE="print"
SHOWPEAK=""
for arg in "$@"; do
    case "$arg" in
        --update) MODE="update" ;;
        --showpeak) SHOWPEAK="1" ;;
        -h|--help)
            echo "Usage: scripts/accuracy_report.sh [--update] [--showpeak]"
            echo "  (no args)   Build test_tdd, run it, print accuracy table"
            echo "  --update    Also patch README.md, docs/README.md, pages/index.html"
            echo "  --showpeak  Add a 'Peak at' column showing the input that produced max % error"
            exit 0
            ;;
        *) echo "Unknown option: $arg" >&2; exit 1 ;;
    esac
done

# -----------------------------------------------------------------------
# 1. Build test_tdd
# -----------------------------------------------------------------------
echo "Building test_tdd..." >&2
make -s dirs
make -s build/test_tdd 2>&1 >&2

# -----------------------------------------------------------------------
# 2. Run and capture the accuracy table
# -----------------------------------------------------------------------
echo "Running test_tdd..." >&2
if [ -n "$SHOWPEAK" ]; then
    OUTPUT=$(FR_SHOWPEAK=1 ./build/test_tdd 2>/dev/null)
else
    OUTPUT=$(./build/test_tdd 2>/dev/null)
fi

# Extract lines between sentinels (inclusive)
TABLE=$(echo "$OUTPUT" | sed -n '/<!-- ACCURACY_TABLE_START -->/,/<!-- ACCURACY_TABLE_END -->/p')

if [ -z "$TABLE" ]; then
    echo "ERROR: Could not find ACCURACY_TABLE_START/END sentinels in output" >&2
    exit 1
fi

# Extract just the data rows (lines starting with |, excluding header and separator)
DATA_ROWS=$(echo "$TABLE" | grep '^|' | tail -n +3)

if [ -z "$DATA_ROWS" ]; then
    echo "ERROR: No data rows found in accuracy table" >&2
    exit 1
fi

echo "$TABLE"

if [ "$MODE" != "update" ]; then
    exit 0
fi

# -----------------------------------------------------------------------
# 3. Patch markdown files (README.md, docs/README.md)
# -----------------------------------------------------------------------
patch_markdown() {
    local file="$1"
    if [ ! -f "$file" ]; then
        echo "  skip: $file not found" >&2
        return
    fi

    # Build replacement block: sentinel + header + separator + data + sentinel
    local replacement
    replacement="<!-- ACCURACY_TABLE_START -->"$'\n'
    replacement+="| Function | Max err (LSB) | Max err (%) | Avg err (%) | Note |"$'\n'
    replacement+="|---|---:|---:|---:|---|"$'\n'
    replacement+="$DATA_ROWS"$'\n'
    replacement+="<!-- ACCURACY_TABLE_END -->"

    # Use perl to replace between sentinels
    perl -0777 -i -pe "
        s{<!-- ACCURACY_TABLE_START -->.*?<!-- ACCURACY_TABLE_END -->}
         {${replacement}}s
    " "$file"

    echo "  patched: $file" >&2
}

patch_markdown "README.md"
patch_markdown "docs/README.md"

# -----------------------------------------------------------------------
# 4. Patch HTML file (pages/index.html)
# -----------------------------------------------------------------------
patch_html() {
    local file="$1"
    if [ ! -f "$file" ]; then
        echo "  skip: $file not found" >&2
        return
    fi

    # Convert markdown data rows to HTML <tr> rows
    local html_rows=""
    while IFS= read -r line; do
        # Strip leading/trailing |, split by |
        local cells
        cells=$(echo "$line" | sed 's/^| //;s/ |$//' | sed 's/ | /\t/g')
        local tr="<tr>"
        while IFS=$'\t' read -r c1 c2 c3 c4 c5; do
            tr+="<td>${c1}</td><td>${c2}</td><td>${c3}</td><td>${c4}</td><td>${c5}</td>"
        done <<< "$cells"
        tr+="</tr>"
        if [ -n "$html_rows" ]; then
            html_rows+=$'\n'
        fi
        html_rows+="$tr"
    done <<< "$DATA_ROWS"

    # Build the replacement block
    local replacement
    replacement="<!-- ACCURACY_TABLE_START -->"$'\n'
    replacement+="<table>"$'\n'
    replacement+="<thead><tr><th>Function</th><th>Max err (LSB)</th><th>Max err (%)</th><th>Avg err (%)</th><th>Note</th></tr></thead>"$'\n'
    replacement+="<tbody>"$'\n'
    replacement+="$html_rows"$'\n'
    replacement+="</tbody>"$'\n'
    replacement+="</table>"$'\n'
    replacement+="<!-- ACCURACY_TABLE_END -->"

    perl -0777 -i -pe "
        s{<!-- ACCURACY_TABLE_START -->.*?<!-- ACCURACY_TABLE_END -->}
         {${replacement}}s
    " "$file"

    echo "  patched: $file" >&2
}

patch_html "pages/index.html"

echo "Accuracy table updated in all doc files." >&2
