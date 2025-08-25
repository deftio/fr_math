#!/bin/bash
# Enhanced size report for FR_Math library
# Builds for multiple architectures and displays a formatted table

set -e

# Colors for output
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Source and build directories
SRC_DIR="src"
BUILD_DIR="build"
TEMP_DIR="build/size_report"

# Create temp directory for builds
mkdir -p "$TEMP_DIR"

# Function to build and get size for an architecture
build_and_size() {
    local arch=$1
    local compiler=$2
    local flags=$3
    local output_file="$TEMP_DIR/FR_math_${arch}.o"
    
    if command -v $compiler >/dev/null 2>&1; then
        # Try to compile
        if $compiler $flags -Isrc -Wall -Os -c $SRC_DIR/FR_math.c -o "$output_file" 2>/dev/null; then
            # Get size in bytes
            local size=$(wc -c < "$output_file" 2>/dev/null || echo "0")
            echo "$size"
        else
            echo "fail"
        fi
    else
        echo "n/a"
    fi
}

# Function to format number with commas
format_number() {
    printf "%'d" $1 2>/dev/null || echo $1
}

echo ""
echo "========================================="
echo "     FR_Math Multi-Architecture Size Report"
echo "========================================="
echo ""
echo "Building for all available architectures..."
echo ""

# Build for each architecture
x86_32_size=$(build_and_size "x86-32" "gcc" "-m32")
x86_64_size=$(build_and_size "x86-64" "gcc" "-m64")
arm32_size=$(build_and_size "arm32" "arm-linux-gnueabihf-gcc" "")
arm64_size=$(build_and_size "arm64" "aarch64-linux-gnu-gcc" "")
m68k_size=$(build_and_size "m68k" "m68k-elf-gcc" "")
riscv32_size=$(build_and_size "riscv32" "riscv32-unknown-elf-gcc" "")
riscv64_size=$(build_and_size "riscv64" "riscv64-unknown-elf-gcc" "")

# Native build
native_arch=$(uname -m)
native_size=$(build_and_size "native" "gcc" "")

# Print formatted table
printf "┌──────────────┬──────────────┬──────────┐\n"
printf "│ Architecture │  Compiler    │   Size   │\n"
printf "├──────────────┼──────────────┼──────────┤\n"

# Function to print a row
print_row() {
    local arch=$1
    local compiler=$2
    local size=$3
    
    if [ "$size" = "n/a" ]; then
        printf "│ %-12s │ %-12s │ %8s │\n" "$arch" "not found" "    -"
    elif [ "$size" = "fail" ]; then
        printf "│ %-12s │ %-12s │ %8s │\n" "$arch" "error" "    -"
    elif [ "$size" = "0" ]; then
        printf "│ %-12s │ %-12s │ %8s │\n" "$arch" "$compiler" "    -"
    else
        printf "│ %-12s │ %-12s │ %'8d │\n" "$arch" "$compiler" "$size"
    fi
}

# Print each architecture
print_row "x86-32" "gcc -m32" "$x86_32_size"
print_row "x86-64" "gcc -m64" "$x86_64_size"
print_row "ARM32" "arm-gcc" "$arm32_size"
print_row "ARM64" "aarch64-gcc" "$arm64_size"
print_row "68k" "m68k-gcc" "$m68k_size"
print_row "RISC-V 32" "riscv32-gcc" "$riscv32_size"
print_row "RISC-V 64" "riscv64-gcc" "$riscv64_size"
printf "├──────────────┼──────────────┼──────────┤\n"
print_row "Native($native_arch)" "gcc" "$native_size"
printf "└──────────────┴──────────────┴──────────┘\n"

# Optimization comparison for native
if [ "$native_size" != "n/a" ] && [ "$native_size" != "fail" ]; then
    echo ""
    echo "Optimization Comparison (Native $native_arch):"
    echo "────────────────────────────────────────"
    
    os_size=$(gcc -Isrc -Wall -Os -c $SRC_DIR/FR_math.c -o "$TEMP_DIR/FR_math_Os.o" 2>/dev/null && wc -c < "$TEMP_DIR/FR_math_Os.o" || echo "0")
    o2_size=$(gcc -Isrc -Wall -O2 -c $SRC_DIR/FR_math.c -o "$TEMP_DIR/FR_math_O2.o" 2>/dev/null && wc -c < "$TEMP_DIR/FR_math_O2.o" || echo "0")
    o3_size=$(gcc -Isrc -Wall -O3 -c $SRC_DIR/FR_math.c -o "$TEMP_DIR/FR_math_O3.o" 2>/dev/null && wc -c < "$TEMP_DIR/FR_math_O3.o" || echo "0")
    o0_size=$(gcc -Isrc -Wall -O0 -c $SRC_DIR/FR_math.c -o "$TEMP_DIR/FR_math_O0.o" 2>/dev/null && wc -c < "$TEMP_DIR/FR_math_O0.o" || echo "0")
    
    printf "  -O0 (none):  %'8d bytes\n" $o0_size
    printf "  -Os (size):  %'8d bytes\n" $os_size
    printf "  -O2 (speed): %'8d bytes\n" $o2_size
    printf "  -O3 (max):   %'8d bytes\n" $o3_size
fi

echo ""
echo "Note: Install cross-compilers for more architectures:"
echo "  Ubuntu/Debian:"
echo "    sudo apt-get install gcc-multilib g++-multilib"
echo "    sudo apt-get install gcc-arm-linux-gnueabihf"
echo "    sudo apt-get install gcc-aarch64-linux-gnu"
echo "    sudo apt-get install gcc-m68k-linux-gnu"
echo ""
echo "  macOS (via brew):"
echo "    brew install arm-gnu-toolchain"
echo "    brew install riscv-gnu-toolchain"
echo ""