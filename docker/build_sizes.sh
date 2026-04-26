#!/usr/bin/env bash
#
# build_sizes.sh — cross-compile FR_math.c for every supported target
# and report code (text section) sizes in two configurations:
#   Core  = compiled with -DFR_CORE_ONLY (math only, no print, no waves)
#   Full  = compiled without FR_CORE_ONLY (everything included)
#
# Run inside the Docker container:
#   docker run --rm -v $(pwd):/src fr-math-sizes bash /src/docker/build_sizes.sh
#
# Output: build/sizes.csv + markdown table to stdout + build/size_table.md

set -euo pipefail

SRC="/src/src/FR_math.c"
INC="-I/src/src"
OUT="/src/build/size_report_docker"
TABLE="/src/build/size_table.md"
CSV="/src/build/sizes.csv"

mkdir -p "${OUT}"

# ── helpers ────────────────────────────────────────────────────────────

# compile_gcc <label> <compiler> <flags...>
# Compiles FR_math.c → .o, extracts text size via `size`.
# Pass extra flags (including -DFR_CORE_ONLY) via the flags argument.
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
    local suffix="$1"  # "" or "_core"
    local extra="$2"   # "" or "-DFR_CORE_ONLY"
    local obj="${OUT}/FR_math_8051${suffix}"

    if ! command -v sdcc >/dev/null 2>&1; then
        echo "n/a"
        return
    fi

    # SDCC produces Intel HEX, not ELF — link with a dummy main to get
    # a .mem report that includes the ROM/FLASH total.
    cat > /tmp/main_8051.c << 'CEOF'
void main(void) {}
CEOF

    if ! sdcc -mmcs51 --std-c99 --opt-code-size ${extra} ${INC} \
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

# ── compile all targets (Core + Full) ────────────────────────────────

# Each entry: target_name, word_width, compile_func args...
# We store results in parallel arrays.
declare -a TARGET_NAMES TARGET_WIDTHS CORE_SIZES FULL_SIZES

add_target() {
    local name="$1"
    local width="$2"
    local core="$3"
    local full="$4"
    TARGET_NAMES+=("${name}")
    TARGET_WIDTHS+=("${width}")
    CORE_SIZES+=("${core}")
    FULL_SIZES+=("${full}")
}

echo "Compiling FR_math.c for all targets (Core + Full)..."
echo ""

# --- ARM targets ---
add_target "RP2040 (Cortex-M0+)" 32 \
    "$(compile_gcc rp2040_core arm-none-eabi-gcc -DFR_CORE_ONLY -mcpu=cortex-m0plus -mthumb)" \
    "$(compile_gcc rp2040_full arm-none-eabi-gcc -mcpu=cortex-m0plus -mthumb)"

add_target "STM32 (Cortex-M4)" 32 \
    "$(compile_gcc stm32_core arm-none-eabi-gcc -DFR_CORE_ONLY -mcpu=cortex-m4 -mthumb -mfloat-abi=soft)" \
    "$(compile_gcc stm32_full arm-none-eabi-gcc -mcpu=cortex-m4 -mthumb -mfloat-abi=soft)"

add_target "Cortex-M0 (Thumb-1)" 32 \
    "$(compile_gcc cm0_core arm-none-eabi-gcc -DFR_CORE_ONLY -mcpu=cortex-m0 -mthumb)" \
    "$(compile_gcc cm0_full arm-none-eabi-gcc -mcpu=cortex-m0 -mthumb)"

# --- RISC-V ---
add_target "RISC-V 32 (rv32im)" 32 \
    "$(compile_gcc rv32_core riscv64-unknown-elf-gcc -DFR_CORE_ONLY -march=rv32im -mabi=ilp32)" \
    "$(compile_gcc rv32_full riscv64-unknown-elf-gcc -march=rv32im -mabi=ilp32)"

# --- Xtensa ---
add_target "ESP32 (Xtensa)" 32 \
    "$(compile_gcc esp32_core xtensa-esp-elf-gcc -DFR_CORE_ONLY)" \
    "$(compile_gcc esp32_full xtensa-esp-elf-gcc)"

# --- 68k ---
add_target "68k" 32 \
    "$(compile_gcc m68k_core m68k-linux-gnu-gcc-12 -DFR_CORE_ONLY)" \
    "$(compile_gcc m68k_full m68k-linux-gnu-gcc-12)"

# --- x86 ---
add_target "x86-32" 32 \
    "$(compile_gcc x86_32_core gcc -DFR_CORE_ONLY -m32)" \
    "$(compile_gcc x86_32_full gcc -m32)"

add_target "x86-64" 64 \
    "$(compile_gcc x86_64_core gcc -DFR_CORE_ONLY -m64)" \
    "$(compile_gcc x86_64_full gcc -m64)"

# --- 16-bit ---
add_target "MSP430" 16 \
    "$(compile_gcc msp430_core msp430-elf-gcc -DFR_CORE_ONLY -mmcu=msp430f5529)" \
    "$(compile_gcc msp430_full msp430-elf-gcc -mmcu=msp430f5529)"

# --- 68HC11 (8-bit) ---
add_target "68HC11" 8 \
    "$(compile_gcc hc11_core m68hc11-gcc -DFR_CORE_ONLY)" \
    "$(compile_gcc hc11_full m68hc11-gcc)"

# --- 8051 ---
add_target "8051 (SDCC)" 8 \
    "$(compile_sdcc _core -DFR_CORE_ONLY)" \
    "$(compile_sdcc _full "")"

# ── write CSV ────────────────────────────────────────────────────────

echo "target,width,core_bytes,full_bytes" > "${CSV}"
for i in "${!TARGET_NAMES[@]}"; do
    echo "${TARGET_NAMES[$i]},${TARGET_WIDTHS[$i]},${CORE_SIZES[$i]},${FULL_SIZES[$i]}" >> "${CSV}"
done

echo "CSV saved to ${CSV}"
echo ""

# ── format output ─────────────────────────────────────────────────────

fmt_size() {
    local val="$1"
    if [[ "${val}" == "n/a" || "${val}" == "fail" || "${val}" == "compiles" ]]; then
        echo "${val}"
    else
        # Format as X.X KB
        local kb
        kb=$(awk "BEGIN { printf \"%.1f\", ${val}/1024.0 }")
        echo "${kb} KB"
    fi
}

# Print markdown table sorted by width then full size ascending
{
    echo "## FR_Math library size (FR_math.c only, \`-Os\`)"
    echo ""
    echo "| Target | Core | Full |"
    echo "|--------|-----:|-----:|"

    # Build sortable lines: width,full_bytes,index
    declare -a SORT_LINES
    for i in "${!TARGET_NAMES[@]}"; do
        local_full="${FULL_SIZES[$i]}"
        # Use 999999 for non-numeric values so they sort last
        if [[ "${local_full}" =~ ^[0-9]+$ ]]; then
            sort_key="${local_full}"
        else
            sort_key="999999"
        fi
        SORT_LINES+=("${TARGET_WIDTHS[$i]} ${sort_key} ${i}")
    done

    # Sort by width ascending then by full size ascending
    sorted=$(printf '%s\n' "${SORT_LINES[@]}" | sort -k1,1n -k2,2n)

    while read -r _width _fsize idx; do
        echo "| ${TARGET_NAMES[$idx]} | $(fmt_size "${CORE_SIZES[$idx]}") | $(fmt_size "${FULL_SIZES[$idx]}") |"
    done <<< "${sorted}"

    echo ""
    echo "All sizes are text-section bytes compiled with \`-Os -ffreestanding\`."
    echo "Core = \`-DFR_CORE_ONLY\` (math only, no print, no waves)."
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
echo "CSV saved to build/sizes.csv"
