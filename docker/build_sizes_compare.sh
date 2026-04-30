#!/usr/bin/env bash
#
# build_sizes_compare.sh — cross-compile FR_math.c with and without FR_tan32.c
# for every supported target, and report the size delta.
#
# Run inside the Docker container:
#   docker run --rm -v $(pwd):/src fr-math-sizes bash /src/docker/build_sizes_compare.sh

set -euo pipefail

SRC_OLD="/src/src/FR_math.c"
SRC_NEW="/src/src/FR_tan32.c"
INC="-I/src/src"
OUT="/src/build/size_compare"

mkdir -p "${OUT}"

# ── helpers ────────────────────────────────────────────────────────────

# get_text_size <compiler> <size-tool> <flags> <sources...>
# Compiles source(s) to .o files, sums .text sections.
get_text_size() {
    local label="$1"; shift
    local cc="$1"; shift
    local sz_cmd="$1"; shift
    local flags="$1"; shift
    # remaining args are source files

    if ! command -v "${cc}" >/dev/null 2>&1; then
        echo "n/a"
        return
    fi

    local total=0
    for src in "$@"; do
        local bname
        bname=$(basename "${src}" .c)
        local obj="${OUT}/${label}_${bname}.o"
        if ! ${cc} ${flags} ${INC} -std=c99 -Wall -Os -ffreestanding \
           -c "${src}" -o "${obj}" 2>/dev/null; then
            echo "fail"
            return
        fi
        local text
        text=$(${sz_cmd} --format=berkeley "${obj}" 2>/dev/null | tail -1 | awk '{print $1}')
        total=$((total + text))
    done
    echo "${total}"
}

# resolve_size_tool: given a compiler path, find the matching size binary
resolve_size_tool() {
    local cc="$1"
    local prefix="${cc%-gcc*}"
    prefix="${prefix%-gcc-*}"
    if [[ "${prefix}" != "${cc}" ]] && command -v "${prefix}-size" >/dev/null 2>&1; then
        echo "${prefix}-size"
    else
        echo "size"
    fi
}

# ── target definitions ────────────────────────────────────────────────

declare -a T_NAMES T_CCS T_SZ T_FLAGS

add() {
    T_NAMES+=("$1")
    T_CCS+=("$2")
    T_SZ+=("$(resolve_size_tool "$2")")
    T_FLAGS+=("$3")
}

# ARM
add "RP2040 (Cortex-M0+)"   arm-none-eabi-gcc  "-mcpu=cortex-m0plus -mthumb"
add "STM32 (Cortex-M4)"     arm-none-eabi-gcc  "-mcpu=cortex-m4 -mthumb -mfloat-abi=soft"
add "Cortex-M0 (Thumb-1)"   arm-none-eabi-gcc  "-mcpu=cortex-m0 -mthumb"

# RISC-V
add "RISC-V 32 (rv32im)"    riscv64-unknown-elf-gcc  "-march=rv32im -mabi=ilp32"

# Xtensa (ESP32)
add "ESP32 (Xtensa)"        xtensa-esp-elf-gcc  ""

# 68k
add "68k"                    m68k-linux-gnu-gcc-12  ""

# x86
add "x86-32"                gcc  "-m32"
add "x86-64"                gcc  "-m64"

# MSP430 (16-bit, no stdint)
add "MSP430"                msp430-elf-gcc  "-mmcu=msp430f5529 -DFR_NO_STDINT"

# 68HC11 (8-bit)
add "68HC11"                m68hc11-gcc  "-DFR_NO_STDINT"

# ── compile ────────────────────────────────────────────────────────────

echo ""
echo "FR_Math cross-platform size comparison: OLD vs OLD+NEW tan32"
echo "Date: $(date -u '+%Y-%m-%d %H:%M UTC')"
echo ""

declare -a R_OLD R_NEW

for i in "${!T_NAMES[@]}"; do
    label="${T_NAMES[$i]}"
    cc="${T_CCS[$i]}"
    sz="${T_SZ[$i]}"
    flags="${T_FLAGS[$i]}"

    tag=$(echo "${label}" | tr ' ()/' '____')

    old=$(get_text_size "${tag}_old" "${cc}" "${sz}" "${flags}" "${SRC_OLD}")
    new=$(get_text_size "${tag}_new" "${cc}" "${sz}" "${flags}" "${SRC_OLD}" "${SRC_NEW}")

    R_OLD+=("${old}")
    R_NEW+=("${new}")

    echo "  ${label}: old=${old}  old+new=${new}"
done

# ── output table ───────────────────────────────────────────────────────

echo ""
echo "## FR_Math size: Old vs Old + 32-bit LUT tan (\`-Os -ffreestanding\`)"
echo ""
printf "| %-26s | %10s | %10s | %10s | %6s |\n" "Target" "Old (text)" "w/ tan32" "Delta" "Delta%"
printf "| %-26s | %10s | %10s | %10s | %6s |\n" "--------------------------" "----------" "----------" "----------" "------"

for i in "${!T_NAMES[@]}"; do
    old="${R_OLD[$i]}"
    new="${R_NEW[$i]}"

    if [[ "${old}" =~ ^[0-9]+$ ]] && [[ "${new}" =~ ^[0-9]+$ ]]; then
        delta=$((new - old))
        pct=$(awk "BEGIN { printf \"%.1f\", 100.0*${delta}/${old} }")
        printf "| %-26s | %8s B | %8s B | %+8d B | %5s%% |\n" \
            "${T_NAMES[$i]}" "${old}" "${new}" "${delta}" "${pct}"
    else
        printf "| %-26s | %10s | %10s | %10s | %6s |\n" \
            "${T_NAMES[$i]}" "${old}" "${new}" "—" "—"
    fi
done

echo ""
echo "Old = FR_math.c only (contains existing tan/atan)."
echo "w/ tan32 = FR_math.c + FR_tan32.c (adds new 32-bit LUT tan/atan alongside old)."
echo "Delta = additional bytes from FR_tan32.c (new functions + 129-entry u32 table)."
echo ""

# ── per-function breakdown (x86-64) ───────────────────────────────────

echo "### Per-function breakdown (x86-64, GCC -Os)"
echo ""

obj_old="${OUT}/x86_64_old_FR_math.o"
obj_new="${OUT}/x86_64_new_FR_tan32.o"

if [[ -f "${obj_old}" ]] && [[ -f "${obj_new}" ]]; then
    echo "**Old tan/atan in FR_math.o:**"
    echo '```'
    nm "${obj_old}" -n -S --size-sort -f sysv -t d 2>/dev/null | grep -iE "tan|atan" || true
    echo '```'
    echo ""
    echo "**New in FR_tan32.o:**"
    echo '```'
    nm "${obj_new}" -n -S --size-sort -f sysv -t d 2>/dev/null | grep -E "FUNC" || true
    echo '```'
fi

echo ""
echo "Done."
