#!/bin/bash
# Coverage report using gcov with pretty ASCII tables
# No external dependencies beyond gcc/gcov

set -e

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

# Directories
SRC_DIR="src"
TEST_DIR="tests"
BUILD_DIR="build"
COV_DIR="coverage"

# Clean and create directories
rm -rf "$BUILD_DIR" "$COV_DIR"
mkdir -p "$BUILD_DIR" "$COV_DIR"

echo ""
echo "========================================="
echo "     FR_Math Coverage Report"
echo "========================================="
echo ""

# Build with coverage flags
echo "Building with coverage flags..."
gcc -Isrc -Wall -Os -fprofile-arcs -ftest-coverage -c $SRC_DIR/FR_math.c -o $BUILD_DIR/FR_math.o
g++ -Isrc -Wall -Os -fprofile-arcs -ftest-coverage -c $SRC_DIR/FR_math_2D.cpp -o $BUILD_DIR/FR_math_2D.o

# Build and run all tests
echo "Running tests..."

# Basic test
gcc -Isrc -Wall -Os -fprofile-arcs -ftest-coverage $TEST_DIR/fr_math_test.c $BUILD_DIR/FR_math.o $BUILD_DIR/FR_math_2D.o -lm -lstdc++ -o $BUILD_DIR/fr_test
$BUILD_DIR/fr_test > /dev/null

# Comprehensive test
if [ -f "$TEST_DIR/test_comprehensive.c" ]; then
    gcc -Isrc -Wall -Os -fprofile-arcs -ftest-coverage $TEST_DIR/test_comprehensive.c $BUILD_DIR/FR_math.o -lm -o $BUILD_DIR/test_comprehensive 2>/dev/null || true
    if [ -f "$BUILD_DIR/test_comprehensive" ]; then
        $BUILD_DIR/test_comprehensive > /dev/null 2>&1 || true
    fi
fi

# 2D math test
if [ -f "$TEST_DIR/test_2d_math.c" ]; then
    gcc -Isrc -Wall -Os -fprofile-arcs -ftest-coverage $TEST_DIR/test_2d_math.c $BUILD_DIR/FR_math.o $BUILD_DIR/FR_math_2D.o -lm -lstdc++ -o $BUILD_DIR/test_2d 2>/dev/null || true
    if [ -f "$BUILD_DIR/test_2d" ]; then
        $BUILD_DIR/test_2d > /dev/null 2>&1 || true
    fi
fi

# Overflow tests
if [ -f "$TEST_DIR/test_overflow_saturation.c" ]; then
    gcc -Isrc -Wall -Os -fprofile-arcs -ftest-coverage $TEST_DIR/test_overflow_saturation.c $BUILD_DIR/FR_math.o -lm -o $BUILD_DIR/test_overflow 2>/dev/null || true
    if [ -f "$BUILD_DIR/test_overflow" ]; then
        $BUILD_DIR/test_overflow > /dev/null 2>&1 || true
    fi
fi

# Full coverage test
if [ -f "$TEST_DIR/test_full_coverage.c" ]; then
    gcc -Isrc -Wall -Os -fprofile-arcs -ftest-coverage $TEST_DIR/test_full_coverage.c $BUILD_DIR/FR_math.o -lm -o $BUILD_DIR/test_full 2>/dev/null || true
    if [ -f "$BUILD_DIR/test_full" ]; then
        $BUILD_DIR/test_full > /dev/null 2>&1 || true
    fi
fi

# 2D complete coverage test
if [ -f "$TEST_DIR/test_2d_complete.cpp" ]; then
    g++ -Isrc -Wall -Os -fprofile-arcs -ftest-coverage $TEST_DIR/test_2d_complete.cpp $BUILD_DIR/FR_math.o $BUILD_DIR/FR_math_2D.o -lm -o $BUILD_DIR/test_2d_complete 2>/dev/null || true
    if [ -f "$BUILD_DIR/test_2d_complete" ]; then
        $BUILD_DIR/test_2d_complete > /dev/null 2>&1 || true
    fi
fi

echo ""
echo "Generating coverage analysis..."
echo ""

# Run gcov and capture output
gcov $SRC_DIR/FR_math.c -o $BUILD_DIR > $BUILD_DIR/coverage_summary.txt 2>&1
gcov $SRC_DIR/FR_math_2D.cpp -o $BUILD_DIR > $BUILD_DIR/coverage_2d_summary.txt 2>&1

# Parse coverage data
parse_coverage() {
    local file=$1
    local gcov_file="${file}.gcov"
    
    if [ -f "$gcov_file" ]; then
        # Count lines more carefully
        local total_lines=0
        local executed_lines=0
        local not_executed=0
        
        while IFS= read -r line; do
            if echo "$line" | grep -q "^[ ]*[0-9][0-9]*:"; then
                ((executed_lines++))
                ((total_lines++))
            elif echo "$line" | grep -q "^[ ]*#####:"; then
                ((not_executed++))
                ((total_lines++))
            fi
        done < "$gcov_file"
        
        local percent=0
        if [ "$total_lines" -gt 0 ]; then
            percent=$((executed_lines * 100 / total_lines))
        fi
        
        echo "$executed_lines $total_lines $percent $not_executed"
    else
        echo "0 0 0 0"
    fi
}

# Get coverage for each file
fr_math_cov=$(parse_coverage "FR_math.c")
fr_math_2d_cov=$(parse_coverage "FR_math_2D.cpp")

# Function to print colored percentage
print_percent() {
    local percent=$1
    if [ "$percent" -ge 80 ]; then
        printf "${GREEN}%3d%%${NC}" $percent
    elif [ "$percent" -ge 50 ]; then
        printf "${YELLOW}%3d%%${NC}" $percent
    else
        printf "${RED}%3d%%${NC}" $percent
    fi
}

# Print coverage table
echo "┌────────────────────────┬──────────┬──────────┬──────────┐"
echo "│ File                   │ Lines    │ Executed │ Coverage │"
echo "├────────────────────────┼──────────┼──────────┼──────────┤"

# FR_math.c coverage
read exec total percent uncov <<< "$fr_math_cov"
printf "│ %-22s │ %8d │ %8d │   " "FR_math.c" $total $exec
print_percent $percent
printf "   │\n"

# FR_math_2D.cpp coverage
read exec total percent uncov <<< "$fr_math_2d_cov"
printf "│ %-22s │ %8d │ %8d │   " "FR_math_2D.cpp" $total $exec
print_percent $percent
printf "   │\n"

echo "└────────────────────────┴──────────┴──────────┴──────────┘"

# Overall summary
fr_math_total=$(echo $fr_math_cov | cut -d' ' -f2)
fr_math_exec=$(echo $fr_math_cov | cut -d' ' -f1)
fr_2d_total=$(echo $fr_math_2d_cov | cut -d' ' -f2)
fr_2d_exec=$(echo $fr_math_2d_cov | cut -d' ' -f1)

total_lines=$((fr_math_total + fr_2d_total))
total_exec=$((fr_math_exec + fr_2d_exec))

if [ "$total_lines" -gt 0 ] && [ "$total_lines" != "0" ]; then
    overall_percent=$((total_exec * 100 / total_lines))
else
    overall_percent=0
fi

echo ""
echo "Overall Coverage: "
printf "  Total Lines:    %d\n" $total_lines
printf "  Lines Executed: %d\n" $total_exec
printf "  Coverage:       "
print_percent $overall_percent
echo ""

# Find untested functions
echo ""
echo "Top Untested Functions (FR_math.c):"
echo "────────────────────────────────────"
if [ -f "$BUILD_DIR/FR_math.c.gcov" ]; then
    # Extract function names that have ##### (not executed) lines
    grep -B5 "#####:" "$BUILD_DIR/FR_math.c.gcov" | grep "^[ ]*[0-9#-]*:.*[a-zA-Z_][a-zA-Z0-9_]*(" | \
        sed 's/^[^:]*:[^:]*://' | \
        grep -oE "[a-zA-Z_][a-zA-Z0-9_]*\(" | \
        sed 's/($//' | \
        sort -u | \
        head -10 | \
        while read func; do
            printf "  • %s\n" "$func"
        done
fi

echo ""
echo "Coverage Details:"
echo "  ✓ Executed lines are covered by tests"
echo "  ✗ Unexecuted lines need test coverage"
echo "  - Lines without executable code"
echo ""
echo "To see detailed line-by-line coverage:"
echo "  cat build/*.gcov"
echo ""

# Exit successfully
exit 0