#!/bin/bash
# FR_Math Test Runner Script
# Easy way to build and run all tests with various configurations

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo "========================================="
echo "     FR_Math Test Runner"
echo "========================================="

# Function to print colored output
print_status() {
    if [ $1 -eq 0 ]; then
        echo -e "${GREEN}✓ $2${NC}"
    else
        echo -e "${RED}✗ $2${NC}"
        exit 1
    fi
}

# Clean previous builds
echo -e "\n${YELLOW}Cleaning previous builds...${NC}"
make clean > /dev/null 2>&1
print_status $? "Clean completed"

# Build library
echo -e "\n${YELLOW}Building library...${NC}"
make lib
print_status $? "Library built"

# Run basic tests
echo -e "\n${YELLOW}Running basic tests...${NC}"
make test
print_status $? "Basic tests passed"

# Run overflow tests if they exist
if [ -f "tests/test_overflow_saturation.c" ]; then
    echo -e "\n${YELLOW}Building overflow/saturation tests...${NC}"
    ${CC:-gcc} -I src -Wall -Os tests/test_overflow_saturation.c src/FR_math.c -lm -o build/test_overflow
    print_status $? "Overflow tests built"
    
    echo -e "${YELLOW}Running overflow/saturation tests...${NC}"
    ./build/test_overflow
    print_status $? "Overflow tests passed"
fi

# Test with different compilers if available
echo -e "\n${YELLOW}Testing with different compilers...${NC}"

# Test with GCC
if command -v gcc &> /dev/null; then
    echo "Testing with GCC..."
    make clean > /dev/null 2>&1
    CC=gcc make test > /dev/null 2>&1
    print_status $? "GCC tests passed"
fi

# Test with Clang
if command -v clang &> /dev/null; then
    echo "Testing with Clang..."
    make clean > /dev/null 2>&1
    CC=clang make test > /dev/null 2>&1
    print_status $? "Clang tests passed"
fi

# Rebuild for size report
make clean > /dev/null 2>&1
make lib > /dev/null 2>&1

# Test 32-bit build if on 64-bit system
if [ "$(uname -m)" = "x86_64" ] && command -v gcc &> /dev/null; then
    echo -e "\n${YELLOW}Testing 32-bit build (if supported)...${NC}"
    make clean > /dev/null 2>&1
    CFLAGS="-m32" CXXFLAGS="-m32" make test > /dev/null 2>&1 || echo "32-bit build not supported on this system"
fi

# Generate coverage report if lcov is available
if command -v lcov &> /dev/null; then
    echo -e "\n${YELLOW}Generating coverage report...${NC}"
    make clean > /dev/null 2>&1
    make coverage > /dev/null 2>&1
    print_status $? "Coverage report generated"
    echo "Coverage report available in coverage/html/index.html"
else
    echo -e "\n${YELLOW}lcov not found, skipping coverage report${NC}"
fi

# Generate size report
echo -e "\n${YELLOW}Build size report:${NC}"
echo "================================"
if [ -d "build" ] && [ "$(ls -A build/*.o 2>/dev/null)" ]; then
    if command -v size &> /dev/null; then
        echo "Object file sizes (text/data/bss):"
        size build/*.o 2>/dev/null
    else
        echo "File sizes:"
        ls -lh build/*.o 2>/dev/null
    fi
    echo ""
    echo "Binary sizes:"
    ls -lh build/fr_test build/test_overflow 2>/dev/null || true
else
    echo "No build artifacts found. Run 'make lib' first."
fi
echo "================================"

echo -e "\n${GREEN}All tests completed successfully!${NC}"
echo ""
echo "Quick commands:"
echo "  make test     - Run basic tests"
echo "  make coverage - Generate coverage report"
echo "  make clean    - Clean build artifacts"
echo "  make lib      - Build library only"