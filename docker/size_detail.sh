#!/usr/bin/env bash
set -euo pipefail

INC="-I/src/src"
FLAGS="-std=c99 -Wall -Os -ffreestanding"
OUT=/tmp/sz
mkdir -p "${OUT}"

do_platform() {
    local label="$1"
    local cc="$2"
    local flags="$3"

    if ! command -v "${cc}" >/dev/null 2>&1; then
        return
    fi

    # Resolve size and nm tools
    local sz_cmd="size"
    local nm_cmd="nm"
    local prefix="${cc%-gcc*}"
    if [ "${prefix}" != "${cc}" ]; then
        command -v "${prefix}-size" >/dev/null 2>&1 && sz_cmd="${prefix}-size"
        command -v "${prefix}-nm" >/dev/null 2>&1 && nm_cmd="${prefix}-nm"
    fi

    # Compile
    ${cc} ${FLAGS} ${flags} ${INC} -c /src/src/FR_math.c  -o "${OUT}/old.o" 2>/dev/null || return
    ${cc} ${FLAGS} ${flags} ${INC} -c /src/src/FR_tan32.c -o "${OUT}/new.o" 2>/dev/null || return

    local old_text new_text
    old_text=$(${sz_cmd} --format=berkeley "${OUT}/old.o" | tail -1 | awk '{print $1}')
    new_text=$(${sz_cmd} --format=berkeley "${OUT}/new.o" | tail -1 | awk '{print $1}')

    # Sum old tan/atan function sizes from nm -S
    local old_tan_total=0
    while IFS=' ' read -r addr size typ name; do
        if [ -n "${size}" ]; then
            dec_size=$((16#${size}))
            old_tan_total=$((old_tan_total + dec_size))
        fi
    done < <(${nm_cmd} -n -S --defined-only "${OUT}/old.o" 2>/dev/null \
             | grep -E " [tT] " | grep -iE "tan|atan" || true)

    local replace_delta=$((new_text - old_tan_total))
    local new_total=$((old_text - old_tan_total + new_text))

    printf "| %-26s | %6s | %6s | %6s | %6s | %+6d |\n" \
        "${label}" "${old_text}" "${old_tan_total}" "${new_text}" "${new_total}" "${replace_delta}"

    rm -f "${OUT}/old.o" "${OUT}/new.o"
}

echo ""
echo "## FR_Math: Old vs Replacement size (new tan32 replaces old tan/atan)"
echo ""
printf "| %-26s | %6s | %6s | %6s | %6s | %6s |\n" \
    "Target" "Old" "OldT/A" "New" "Repl" "Delta"
printf "| %-26s | %6s | %6s | %6s | %6s | %6s |\n" \
    "--------------------------" "------" "------" "------" "------" "------"

do_platform "RP2040 (Cortex-M0+)"     arm-none-eabi-gcc      "-mcpu=cortex-m0plus -mthumb"
do_platform "STM32 (Cortex-M4)"       arm-none-eabi-gcc      "-mcpu=cortex-m4 -mthumb -mfloat-abi=soft"
do_platform "Cortex-M0 (Thumb-1)"     arm-none-eabi-gcc      "-mcpu=cortex-m0 -mthumb"
do_platform "RISC-V 32 (rv32im)"      riscv64-unknown-elf-gcc "-march=rv32im -mabi=ilp32"
do_platform "ESP32 (Xtensa)"          xtensa-esp-elf-gcc     ""
do_platform "68k"                      m68k-linux-gnu-gcc-12  ""
do_platform "x86-32"                   gcc                    "-m32"
do_platform "x86-64"                   gcc                    "-m64"
do_platform "MSP430"                   msp430-elf-gcc         "-mmcu=msp430f5529 -DFR_NO_STDINT"

echo ""
echo "Old     = FR_math.c total .text"
echo "OldT/A  = old tan+atan functions within FR_math.o (would be removed)"
echo "New     = FR_tan32.c total .text (replacement functions + 129-entry u32 table)"
echo "Repl    = library size after replacement (Old - OldT/A + New)"
echo "Delta   = New - OldT/A (net change from replacement)"

# === x86-64 per-function detail ===
echo ""
echo "### x86-64 per-function detail"
echo ""

gcc ${FLAGS} -m64 ${INC} -c /src/src/FR_math.c  -o "${OUT}/old.o" 2>/dev/null
gcc ${FLAGS} -m64 ${INC} -c /src/src/FR_tan32.c -o "${OUT}/new.o" 2>/dev/null

echo "**Old tan/atan functions in FR_math.o:**"
echo '```'
nm -n -S --defined-only "${OUT}/old.o" | grep -E " [tT] " | grep -iE "tan|atan" | \
while IFS=' ' read -r addr size typ name; do
    printf "  %-30s %d bytes\n" "${name}" "$((16#${size}))"
done
echo '```'

echo ""
echo "**New functions in FR_tan32.o:**"
echo '```'
nm -n -S --defined-only "${OUT}/new.o" | grep -E " [tT] " | \
while IFS=' ' read -r addr size typ name; do
    printf "  %-30s %d bytes\n" "${name}" "$((16#${size}))"
done
echo '```'
