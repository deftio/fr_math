#!/usr/bin/env bash
#
# crossbuild_sizes.sh — cross-compile FR_math inside Docker, generate size
# tables, and optionally patch doc files.
#
# Usage:
#   scripts/crossbuild_sizes.sh            # build, print table, write CSV + MD
#   scripts/crossbuild_sizes.sh --update   # also patch doc files
#
# Requires: docker, xelp-crossbuild:latest image
#
# Output files:
#   build/sizes.csv  — raw CSV (target,lean,core,full)
#   build/sizes.md   — markdown table
#
# With --update, patches these files between <!-- SIZE_TABLE_START/END --> sentinels:
#   README.md              — markdown table
#   docs/building.md       — markdown table
#   pages/guide/building.html — HTML <table>

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
cd "${PROJECT_ROOT}"

MODE="print"
for arg in "$@"; do
    case "$arg" in
        --update) MODE="update" ;;
        -h|--help)
            echo "Usage: scripts/crossbuild_sizes.sh [--update]"
            echo "  (no args)   Build in Docker, print size table, write CSV + MD"
            echo "  --update    Also patch README.md, docs/building.md, pages/guide/building.html"
            exit 0
            ;;
        *) echo "Unknown option: $arg" >&2; exit 1 ;;
    esac
done

# -----------------------------------------------------------------------
# 1. Preflight checks
# -----------------------------------------------------------------------

if ! command -v docker >/dev/null 2>&1; then
    echo "ERROR: docker not found. Install Docker first." >&2
    exit 1
fi

if ! docker image inspect xelp-crossbuild:latest >/dev/null 2>&1; then
    echo "ERROR: Docker image 'xelp-crossbuild:latest' not found." >&2
    echo "Build it with: docker build -t xelp-crossbuild:latest scripts/" >&2
    exit 1
fi

mkdir -p build

# -----------------------------------------------------------------------
# 2. Run cross-compilation inside Docker
# -----------------------------------------------------------------------

echo "Running cross-compilation in Docker..."

docker run --rm -v "${PROJECT_ROOT}:/fr_math" xelp-crossbuild:latest \
    /bin/bash -c '
set -e

SRC=/fr_math/src/FR_math.c
INCLUDE="-I/fr_math/src"
OBJ=/tmp/FR_math.o
CSV=/fr_math/build/sizes.csv

LEAN_DEFS="-DFR_LEAN -DFR_NO_PRINT"
CORE_DEFS="-DFR_CORE_ONLY"
FULL_DEFS=""

build_text_size() {
    local compiler="$1"
    local flags="$2"
    local defs="$3"
    rm -f "$OBJ"
    if $compiler -c $SRC $INCLUDE $flags $defs -Os -Wall -o $OBJ 2>/dev/null; then
        size "$OBJ" 2>/dev/null | awk "FNR==2{print \$1}"
    else
        echo "FAIL"
    fi
    rm -f "$OBJ"
}

build_target() {
    local label="$1"
    local compiler="$2"
    local flags="$3"

    local lean_sz=$(build_text_size "$compiler" "$flags" "$LEAN_DEFS")
    local core_sz=$(build_text_size "$compiler" "$flags" "$CORE_DEFS")
    local full_sz=$(build_text_size "$compiler" "$flags" "$FULL_DEFS")
    echo "${label},${lean_sz},${core_sz},${full_sz}" >> "$CSV"
}

# Write CSV header
echo "target,lean,core,full" > "$CSV"

# --- x86 ---
build_target "GCC x86-64"                "gcc"                        ""
build_target "Clang x86-64"              "clang"                      ""
build_target "GCC x86-32"                "gcc"                        "-m32"
build_target "TCC x86"                   "tcc"                        ""

# --- ARM ---
build_target "GCC AArch64 (ARM64)"       "aarch64-linux-gnu-gcc"      ""
build_target "GCC ARM32"                 "arm-none-eabi-gcc"          ""
build_target "GCC ARM Thumb"             "arm-none-eabi-gcc"          "-mthumb"
build_target "Cortex-M0 (RP2040)"        "arm-none-eabi-gcc"          "-mcpu=cortex-m0 -mthumb"
build_target "Cortex-M4 (STM32)"         "arm-none-eabi-gcc"          "-mcpu=cortex-m4 -mthumb"

# --- Bare-metal: no stdint.h in sysroot ---
NOSTD="-DFR_NO_STDINT"

# --- MSP430 ---
build_target "GCC MSP430"               "msp430-gcc"                 "$NOSTD"

# --- AVR ---
build_target "AVR ATmega328P"            "avr-gcc"                    "$NOSTD -mmcu=avr5"
build_target "AVR ATtiny85"              "avr-gcc"                    "$NOSTD -mmcu=attiny85"

# --- 68HC11 ---
build_target "GCC 68HC11"               "m68hc11-gcc"                "$NOSTD"

# --- 68k ---
build_target "GCC m68k"                 "m68k-linux-gnu-gcc"         ""

# --- PowerPC ---
build_target "GCC PowerPC"              "powerpc-linux-gnu-gcc"      ""

# --- RISC-V ---
build_target "RISC-V rv64"              "riscv64-linux-gnu-gcc"      ""
build_target "RISC-V rv32"              "riscv64-unknown-elf-gcc"    "$NOSTD -march=rv32imac -mabi=ilp32"

# --- Xtensa ---
build_target "Xtensa LX106 (ESP8266)"   "xtensa-lx106-elf-gcc"      "$NOSTD"

echo "Docker build complete — $(grep -c , "$CSV") rows written to build/sizes.csv"
'

# -----------------------------------------------------------------------
# 3. Generate tables on host
# -----------------------------------------------------------------------

CSV="build/sizes.csv"

if [ ! -f "${CSV}" ]; then
    echo "ERROR: ${CSV} not found after Docker run." >&2
    exit 1
fi

# Sort by full size ascending (skip header)
SORTED=$(tail -n +2 "${CSV}" | sort -t',' -k4,4n)

if [ -z "${SORTED}" ]; then
    echo "ERROR: No data rows in ${CSV}" >&2
    exit 1
fi

# Format bytes as X.X KB using integer math (no bc dependency)
fmt_kb() {
    local val="$1"
    if [[ "${val}" =~ ^[0-9]+$ ]]; then
        local whole=$((val / 1024))
        local frac=$(( (val % 1024) * 10 / 1024 ))
        echo "${whole}.${frac} KB"
    else
        echo "${val}"
    fi
}

# --- Console summary ---
echo ""
echo "============================================================"
echo "FR_math.c code size (.text bytes), compiled with -Os"
echo "============================================================"
echo ""
printf "  %-28s  %8s  %8s  %8s\n" "Target" "Lean" "Core" "Full"
printf "  %-28s  %8s  %8s  %8s\n" "----------------------------" "--------" "--------" "--------"
while IFS=',' read -r target lean core full; do
    printf "  %-28s  %8s  %8s  %8s\n" "$target" "$lean" "$core" "$full"
done <<< "${SORTED}"
echo ""
echo "Lean = -DFR_LEAN -DFR_NO_PRINT (radian trig, inv trig, log/exp, sqrt)"
echo "Core = -DFR_CORE_ONLY          (+ degree trig, BAM tan, log10, hypot)"
echo "Full = all features             (+ print, waves, ADSR)"
echo ""

# --- build/sizes.md ---
{
    echo "# FR_math.c Code Sizes (.text bytes, -Os)"
    echo ""
    echo "| Target | Lean | Core | Full |"
    echo "|--------|-----:|-----:|-----:|"
    while IFS=',' read -r target lean core full; do
        printf "| %s | %s | %s | %s |\n" "$target" "$(fmt_kb "$lean")" "$(fmt_kb "$core")" "$(fmt_kb "$full")"
    done <<< "${SORTED}"
    echo ""
    echo "Lean = \`-DFR_LEAN -DFR_NO_PRINT\` | Core = \`-DFR_CORE_ONLY\` | Full = all features"
} > build/sizes.md

echo "Wrote build/sizes.csv and build/sizes.md"

if [ "${MODE}" != "update" ]; then
    exit 0
fi

# -----------------------------------------------------------------------
# 4. Patch doc files
# -----------------------------------------------------------------------

# Build markdown replacement block
MD_ROWS=""
while IFS=',' read -r target lean core full; do
    row="| ${target} | $(fmt_kb "${lean}") | $(fmt_kb "${core}") | $(fmt_kb "${full}") |"
    if [ -n "${MD_ROWS}" ]; then
        MD_ROWS+=$'\n'
    fi
    MD_ROWS+="${row}"
done <<< "${SORTED}"

MD_TABLE="<!-- SIZE_TABLE_START -->"$'\n'
MD_TABLE+="| Target | Lean | Core | Full |"$'\n'
MD_TABLE+="|--------|-----:|-----:|-----:|"$'\n'
MD_TABLE+="${MD_ROWS}"$'\n'
MD_TABLE+="<!-- SIZE_TABLE_END -->"

# Patch a markdown file between sentinels
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

    echo "  patched: $file"
}

# Patch HTML file between sentinels
patch_html() {
    local file="$1"
    if [ ! -f "$file" ]; then
        echo "  skip: $file not found" >&2
        return
    fi

    # Build HTML rows
    local html_rows=""
    while IFS=',' read -r target lean core full; do
        local tr="<tr><td>${target}</td><td>$(fmt_kb "${lean}")</td><td>$(fmt_kb "${core}")</td><td>$(fmt_kb "${full}")</td></tr>"
        if [ -n "$html_rows" ]; then
            html_rows+=$'\n'
        fi
        html_rows+="${tr}"
    done <<< "${SORTED}"

    local replacement
    replacement="<!-- SIZE_TABLE_START -->"$'\n'
    replacement+="<table>"$'\n'
    replacement+="<thead><tr><th>Target</th><th>Lean</th><th>Core</th><th>Full</th></tr></thead>"$'\n'
    replacement+="<tbody>"$'\n'
    replacement+="${html_rows}"$'\n'
    replacement+="</tbody>"$'\n'
    replacement+="</table>"$'\n'
    replacement+="<!-- SIZE_TABLE_END -->"

    perl -0777 -i -pe "
        s{<!-- SIZE_TABLE_START -->.*?<!-- SIZE_TABLE_END -->}
         {${replacement}}s
    " "$file"

    echo "  patched: $file"
}

echo ""
echo "Patching doc files..."
patch_markdown "README.md"
patch_markdown "docs/building.md"
patch_html "pages/guide/building.html"
echo "Done."
