#!/bin/bash
# crossbuild-docker.sh -- cross-compile FR_math inside Docker container
# Runs inside the xelp-crossbuild Docker image.
# Reports object file and .text section sizes for each target.

set -e

SRC=/fr_math/src/FR_math.c
INCLUDE="-I/fr_math/src"
OBJ=/tmp/FR_math.o

SEP="============================================================"

# Accumulate summary rows: "label|text_size"
SUMMARY=""

print_sizes() {
    local label="$1"
    echo ""
    echo "$SEP"
    echo "$label"
    echo "$SEP"
    if [ ! -f "$OBJ" ]; then
        echo "  (build failed)"
        SUMMARY="${SUMMARY}${label}|FAIL\n"
        return
    fi
    OBJ_SIZE=$(stat -c%s "$OBJ" 2>/dev/null || wc -c < "$OBJ")
    TEXT_SIZE=$(size "$OBJ" 2>/dev/null | awk 'FNR==2{print $1}')
    printf "  obj file size: %6s bytes\n" "$OBJ_SIZE"
    printf "  .text section: %6s bytes\n" "$TEXT_SIZE"
    SUMMARY="${SUMMARY}${label}|${TEXT_SIZE}\n"
    rm -f "$OBJ"
}

echo ""
echo "FR_Math cross-compilation size report"
echo "Date: $(date -u '+%Y-%m-%d %H:%M UTC')"
echo ""

# --- x86 ---
gcc -c $SRC $INCLUDE -Os -Wall -o $OBJ 2>&1 && true
print_sizes "GCC x86-64"

clang -c $SRC $INCLUDE -Os -Wall -o $OBJ 2>&1 && true
print_sizes "Clang x86-64"

gcc -c $SRC $INCLUDE -Os -m32 -Wall -o $OBJ 2>&1 && true
print_sizes "GCC x86-32"

tcc -c $SRC $INCLUDE -o $OBJ 2>&1 && true
print_sizes "TCC x86"

# --- ARM ---
aarch64-linux-gnu-gcc -c $SRC $INCLUDE -Os -Wall -o $OBJ 2>&1 && true
print_sizes "GCC AArch64 (ARM64)"

arm-none-eabi-gcc -c $SRC $INCLUDE -Os -Wall -o $OBJ 2>&1 && true
print_sizes "GCC ARM32"

arm-none-eabi-gcc -c $SRC $INCLUDE -Os -mthumb -Wall -o $OBJ 2>&1 && true
print_sizes "GCC ARM32 Thumb"

# --- MSP430 ---
# Bare-metal: no stdint.h in sysroot — use fallback typedefs
NOSTD="-DFR_NO_STDINT"

msp430-gcc -c $SRC $INCLUDE $NOSTD -Os -Wall -o $OBJ 2>&1 && true
print_sizes "GCC MSP430"

# --- AVR ---
avr-gcc -c $SRC $INCLUDE $NOSTD -Os -mmcu=avr5 -Wall -o $OBJ 2>&1 && true
print_sizes "GCC AVR5 (ATmega328P)"

avr-gcc -c $SRC $INCLUDE $NOSTD -Os -mmcu=attiny85 -Wall -o $OBJ 2>&1 && true
print_sizes "GCC AVR ATtiny85"

# --- 68HC11 ---
m68hc11-gcc -c $SRC $INCLUDE $NOSTD -Os -o $OBJ 2>&1 && true
print_sizes "GCC 68HC11"

# --- 68k (Motorola 68000) ---
m68k-linux-gnu-gcc -c $SRC $INCLUDE -Os -Wall -o $OBJ 2>&1 && true
print_sizes "GCC m68k"

# --- PowerPC ---
powerpc-linux-gnu-gcc -c $SRC $INCLUDE -Os -Wall -o $OBJ 2>&1 && true
print_sizes "GCC PowerPC"

# --- RISC-V ---
riscv64-linux-gnu-gcc -c $SRC $INCLUDE -Os -Wall -o $OBJ 2>&1 && true
print_sizes "GCC RISC-V (rv64)"

riscv64-unknown-elf-gcc -c $SRC $INCLUDE $NOSTD -Os -march=rv32imac -mabi=ilp32 -Wall -o $OBJ 2>&1 && true
print_sizes "GCC RISC-V (rv32)"

# --- Xtensa (ESP8266/ESP32 family) ---
xtensa-lx106-elf-gcc -c $SRC $INCLUDE $NOSTD -Os -Wall -o $OBJ 2>&1 && true
print_sizes "GCC Xtensa LX106 (ESP8266)"

# --- Function size table (native GCC) ---
echo ""
echo "$SEP"
echo "Function size table (GCC x86-64)"
echo "$SEP"
gcc -c $SRC $INCLUDE -Os -Wall -o $OBJ 2>&1
nm $OBJ -n -S --size-sort -f sysv -t d 2>/dev/null | grep -E "FUNC" || true
rm -f $OBJ

# --- Summary table ---
echo ""
echo "$SEP"
echo "Summary: FR_math.c code size (bytes), compiled with -Os"
echo "$SEP"
printf "  %-28s %s\n" "Target" ".text (bytes)"
printf "  %-28s %s\n" "----------------------------" "-------------"
echo -e "$SUMMARY" | while IFS='|' read -r label size; do
    [ -z "$label" ] && continue
    printf "  %-28s %s\n" "$label" "$size"
done

echo ""
echo "Done."
