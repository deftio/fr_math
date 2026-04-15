#!/usr/bin/env bash
#
# build_sizes.sh — cross-compile FR_math.c for every supported target
# and report code (text section) sizes.
#
# Run inside the Docker container:
#   docker run --rm -v $(pwd):/src fr-math-sizes bash /src/docker/build_sizes.sh
#
# Output: markdown table to stdout + build/size_table.md

set -euo pipefail

SRC="/src/src/FR_math.c"
INC="-I/src/src"
OUT="/src/build/size_report_docker"
TABLE="/src/build/size_table.md"

mkdir -p "${OUT}"

# ── helpers ────────────────────────────────────────────────────────────

# compile_gcc <label> <compiler> <flags...>
# Compiles FR_math.c → .o, extracts text size via `size`.
compile_gcc() {
    local label="$1"; shift
    local cc="$1"; shift
    local flags="$*"
    local obj="${OUT}/FR_math_${label}.o"

    if ! command -v "${cc}" >/dev/null 2>&1; then
        echo "n/a"
        return
    fi

    if ${cc} ${flags} ${INC} -std=c99 -Wall -Os -ffreestanding \
       -c "${SRC}" -o "${obj}" 2>/dev/null; then
        # Use the matching size tool if available, else default `size`.
        local sz_cmd="size"
        local prefix="${cc%-gcc*}"
        prefix="${prefix%-gcc-*}"
        if [[ "${prefix}" != "${cc}" ]] && command -v "${prefix}-size" >/dev/null 2>&1; then
            sz_cmd="${prefix}-size"
        fi
        # GNU size --format=berkeley: text data bss dec hex filename
        local text
        text=$(${sz_cmd} --format=berkeley "${obj}" 2>/dev/null | tail -1 | awk '{print $1}')
        echo "${text}"
    else
        echo "fail"
    fi
}

# compile_sdcc — SDCC uses different flags and output format.
compile_sdcc() {
    local obj="${OUT}/FR_math_8051"

    if ! command -v sdcc >/dev/null 2>&1; then
        echo "n/a"
        return
    fi

    # SDCC produces Intel HEX, not ELF — link with a dummy main to get
    # a .mem report that includes the ROM/FLASH total.
    cat > /tmp/main_8051.c << 'CEOF'
void main(void) {}
CEOF

    if ! sdcc -mmcs51 --std-c99 --opt-code-size ${INC} \
         -c "${SRC}" -o "${obj}.rel" 2>/dev/null; then
        echo "fail"
        return
    fi

    sdcc -mmcs51 --std-c99 --opt-code-size \
         -c /tmp/main_8051.c -o /tmp/main_8051.rel 2>/dev/null

    # Link may fail (8051 internal RAM is tiny) but .mem is still written.
    sdcc -mmcs51 --opt-code-size \
         /tmp/main_8051.rel "${obj}.rel" \
         -o "${obj}.ihx" 2>/dev/null || true

    local rom
    rom=$(grep 'ROM/EPROM/FLASH' "${obj}.mem" 2>/dev/null | awk '{print $4}')
    if [[ -n "${rom}" && "${rom}" != "0" ]]; then
        echo "${rom}"
    else
        echo "compiles"
    fi
}

# ── compile all targets ───────────────────────────────────────────────

declare -A RESULTS
declare -a ORDER

add_result() {
    local label="$1"
    local value="$2"
    RESULTS["${label}"]="${value}"
    ORDER+=("${label}")
}

echo "Compiling FR_math.c for all targets..."
echo ""

add_result "Cortex-M0 (Thumb-1)" \
    "$(compile_gcc cm0 arm-none-eabi-gcc -mcpu=cortex-m0 -mthumb)"

add_result "Cortex-M4 (Thumb-2)" \
    "$(compile_gcc cm4 arm-none-eabi-gcc -mcpu=cortex-m4 -mthumb -mfloat-abi=soft)"

add_result "MSP430" \
    "$(compile_gcc msp430 msp430-elf-gcc -mmcu=msp430f5529)"

add_result "RISC-V 32 (rv32im)" \
    "$(compile_gcc rv32 riscv64-unknown-elf-gcc -march=rv32im -mabi=ilp32)"

add_result "ESP32 (Xtensa)" \
    "$(compile_gcc esp32 xtensa-esp-elf-gcc)"

add_result "68k" \
    "$(compile_gcc m68k m68k-linux-gnu-gcc-12)"

add_result "x86-32" \
    "$(compile_gcc x86_32 gcc -m32)"

add_result "x86-64" \
    "$(compile_gcc x86_64 gcc -m64)"

add_result "8051 (SDCC)" \
    "$(compile_sdcc)"

# ── format output ─────────────────────────────────────────────────────

fmt_size() {
    local val="$1"
    if [[ "${val}" == "n/a" || "${val}" == "fail" || "${val}" == "compiles" ]]; then
        echo "${val}"
    else
        # Format as X.X KB
        local kb
        kb=$(awk "BEGIN { printf \"%.1f\", ${val}/1024.0 }")
        echo "${val} B (${kb} KB)"
    fi
}

# Print to stdout and file
{
    echo "## FR_Math library size (FR_math.c only, \`-Os\`)"
    echo ""
    echo "| Target | Code (text) |"
    echo "|--------|-------------|"
    for label in "${ORDER[@]}"; do
        val="${RESULTS[${label}]}"
        echo "| ${label} | $(fmt_size "${val}") |"
    done
    echo ""
    echo "All sizes are text-section bytes compiled with \`-Os -ffreestanding\`."
    echo "The optional 2D module (\`FR_math_2D.cpp\`) adds ~1 KB."
} | tee "${TABLE}"

# ── optimization comparison (Cortex-M0) ──────────────────────────────

echo ""
echo "### Optimization comparison (Cortex-M0)"
echo ""
echo "| Flag | Code (text) |"
echo "|------|-------------|"

for opt in O0 Os O2 O3; do
    obj="${OUT}/FR_math_cm0_${opt}.o"
    arm-none-eabi-gcc -mcpu=cortex-m0 -mthumb \
        ${INC} -std=c99 -Wall -${opt} -ffreestanding \
        -c "${SRC}" -o "${obj}" 2>/dev/null
    text=$(arm-none-eabi-size --format=berkeley "${obj}" 2>/dev/null | tail -1 | awk '{print $1}')
    kb=$(awk "BEGIN { printf \"%.1f\", ${text}/1024.0 }")
    echo "| -${opt} | ${text} B (${kb} KB) |"
done

# ── library comparison (Cortex-M0, -Os) ──────────────────────────────
# Compiles FR_Math and libfixmath for Cortex-M0 so all numbers come
# from the same toolchain, same flags, same target.

LFM_DIR="/src/.compare/libfixmath/libfixmath"
LFM_INC="-I${LFM_DIR}"
ARM_FLAGS="-mcpu=cortex-m0 -mthumb -std=c99 -Wall -Os -ffreestanding"

echo ""
echo "### Library comparison (Cortex-M0, -Os, arm-none-eabi-gcc)"
echo ""

if [[ -d "${LFM_DIR}" ]] && command -v arm-none-eabi-gcc >/dev/null 2>&1; then
    # FR_Math
    fr_obj="${OUT}/FR_math_cmp_cm0.o"
    arm-none-eabi-gcc ${ARM_FLAGS} ${INC} -c "${SRC}" -o "${fr_obj}" 2>/dev/null
    fr_text=$(arm-none-eabi-size --format=berkeley "${fr_obj}" 2>/dev/null | tail -1 | awk '{print $1}')

    # libfixmath — compile each source file, sum text sections
    lfm_total=0
    LFM_SRCS="fix16.c fix16_sqrt.c fix16_exp.c fix16_trig.c fix16_str.c uint32.c fract32.c"
    for src in ${LFM_SRCS}; do
        obj="${OUT}/lfm_${src%.c}.o"
        arm-none-eabi-gcc ${ARM_FLAGS} ${LFM_INC} \
            -c "${LFM_DIR}/${src}" -o "${obj}" 2>/dev/null
        text=$(arm-none-eabi-size --format=berkeley "${obj}" 2>/dev/null | tail -1 | awk '{print $1}')
        lfm_total=$((lfm_total + text))
    done

    fr_kb=$(awk "BEGIN { printf \"%.1f\", ${fr_text}/1024.0 }")
    lfm_kb=$(awk "BEGIN { printf \"%.1f\", ${lfm_total}/1024.0 }")

    echo "| Library | Code (text) |"
    echo "|---------|-------------|"
    echo "| FR_Math | ${fr_text} B (${fr_kb} KB) |"
    echo "| libfixmath | ${lfm_total} B (${lfm_kb} KB) |"
else
    echo "(skipped — libfixmath not found in .compare/ or arm-none-eabi-gcc not available)"
fi

echo ""
echo "Size table saved to build/size_table.md"
