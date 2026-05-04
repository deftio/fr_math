# FR_Math - Fixed Radix Math Library Makefile
# @author M A Chatterjee <deftio [at] deftio [dot] com>

# Compiler configuration
CC ?= gcc
CXX ?= g++
AR = ar
SIZE = size

# Directories
SRC_DIR = src
TEST_DIR = tests
EXAMPLE_DIR = examples
BUILD_DIR = build
COV_DIR = coverage

# Compiler flags — full warnings, fail on any warning
# LIB_WARN: strictest for library source (includes -Wconversion -Wpedantic)
# CFLAGS:   for tests/examples (no -Wconversion/-Wpedantic — macro casts are intentional)
LIB_WARN = -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Werror
CFLAGS   = -I$(SRC_DIR) -Wall -Wextra -Wshadow -Werror -Os
CXXFLAGS = $(CFLAGS)
TEST_FLAGS = -ftest-coverage -fprofile-arcs
LDFLAGS = -lm

# Source files
HEADERS = $(SRC_DIR)/FR_defs.h $(SRC_DIR)/FR_math.h $(SRC_DIR)/FR_math_2D.h

# Default target — print help
.PHONY: help
help:
	@echo "FR_Math — Fixed Radix Math Library"
	@echo ""
	@echo "Usage: make <target>"
	@echo ""
	@echo "Build targets:"
	@echo "  all              Build library and examples"
	@echo "  lib              Build library objects only"
	@echo "  examples         Build example program"
	@echo ""
	@echo "Test targets:"
	@echo "  test             Run all tests"
	@echo "  test-basic       Run basic tests"
	@echo "  test-comprehensive  Run comprehensive tests"
	@echo "  test-2d          Run 2D math tests"
	@echo "  test-overflow    Run overflow/saturation tests"
	@echo "  test-full        Run full coverage tests"
	@echo "  test-2d-complete Run 2D complete coverage tests"
	@echo "  test-tdd         Run TDD characterization tests"
	@echo ""
	@echo "Analysis targets:"
	@echo "  accuracy         Show accuracy summary table"
	@echo "  accuracy-showpeak  Show accuracy with peak inputs"
	@echo "  coverage         Generate coverage report (gcov)"
	@echo "  coverage-basic   Basic coverage info without lcov"
	@echo "  coverage-html    HTML coverage report (requires lcov)"
	@echo "  size-report      Multi-architecture size report (Docker)"
	@echo "  size-update      Size report + patch doc files"
	@echo "  size-simple      Size report for current platform"
	@echo ""
	@echo "Tools:"
	@echo "  tools            Build diagnostic tools"
	@echo "  trig-neighborhood  Build function neighborhood explorer"
	@echo ""
	@echo "Maintenance:"
	@echo "  clean            Remove build artifacts"
	@echo "  cleanall         Remove build artifacts and backups"

.PHONY: all
all: dirs lib examples

# Create build directories and emit version string from FR_MATH_VERSION_HEX
.PHONY: dirs
dirs:
	@mkdir -p $(BUILD_DIR) $(COV_DIR)
	@RAW=$$(grep '#define FR_MATH_VERSION_HEX' $(SRC_DIR)/FR_math.h | awk '{print $$3}'); \
	 HEX=$$((RAW)); \
	 MAJ=$$(( (HEX >> 16) & 0xff )); \
	 MIN=$$(( (HEX >> 8)  & 0xff )); \
	 PAT=$$(( HEX & 0xff )); \
	 echo "$${MAJ}.$${MIN}.$${PAT}" > $(BUILD_DIR)/version.txt

# Build library
.PHONY: lib
lib: dirs $(BUILD_DIR)/FR_math.o $(BUILD_DIR)/FR_math_2D.o

$(BUILD_DIR)/FR_math.o: $(SRC_DIR)/FR_math.c $(HEADERS)
	$(CC) -I$(SRC_DIR) $(LIB_WARN) -Os -c $< -o $@

$(BUILD_DIR)/FR_math_2D.o: $(SRC_DIR)/FR_math_2D.cpp $(HEADERS)
	$(CXX) -I$(SRC_DIR) $(LIB_WARN) -Os -c $< -o $@

# Build examples
.PHONY: examples
examples: dirs $(BUILD_DIR)/fr_example

$(BUILD_DIR)/fr_example: $(EXAMPLE_DIR)/posix-example/FR_Math_Example1.cpp $(BUILD_DIR)/FR_math.o $(BUILD_DIR)/FR_math_2D.o
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) -o $@

# Build and run tests
.PHONY: test
test: dirs examples test-basic test-comprehensive test-2d test-overflow test-full test-2d-complete test-tdd

.PHONY: test-tdd
test-tdd: $(BUILD_DIR)/test_tdd
	@echo "Running TDD characterization tests..."
	@./$(BUILD_DIR)/test_tdd > $(BUILD_DIR)/test_tdd_report.md
	@echo "Report written to $(BUILD_DIR)/test_tdd_report.md"

$(BUILD_DIR)/test_tdd: $(TEST_DIR)/test_tdd.cpp $(SRC_DIR)/FR_math.c $(SRC_DIR)/FR_math_2D.cpp
	$(CC) -I$(SRC_DIR) $(LIB_WARN) -Os $(TEST_FLAGS) -c $(SRC_DIR)/FR_math.c -o $(BUILD_DIR)/test_tdd_FR_math.o
	$(CXX) -I$(SRC_DIR) $(LIB_WARN) -Os $(TEST_FLAGS) -c $(SRC_DIR)/FR_math_2D.cpp -o $(BUILD_DIR)/test_tdd_FR_math_2D.o
	$(CXX) $(CXXFLAGS) $(TEST_FLAGS) $(TEST_DIR)/test_tdd.cpp $(BUILD_DIR)/test_tdd_FR_math.o $(BUILD_DIR)/test_tdd_FR_math_2D.o $(LDFLAGS) -o $@

.PHONY: test-basic
test-basic: $(BUILD_DIR)/fr_test
	@echo "Running basic tests..."
	@./$(BUILD_DIR)/fr_test

.PHONY: test-comprehensive
test-comprehensive: $(BUILD_DIR)/test_comprehensive
	@echo "Running comprehensive tests..."
	@./$(BUILD_DIR)/test_comprehensive

.PHONY: test-2d
test-2d: $(BUILD_DIR)/test_2d
	@echo "Running 2D math tests..."
	@./$(BUILD_DIR)/test_2d

.PHONY: test-overflow
test-overflow: $(BUILD_DIR)/test_overflow
	@echo "Running overflow tests..."
	@./$(BUILD_DIR)/test_overflow

.PHONY: test-full
test-full: $(BUILD_DIR)/test_full
	@echo "Running full coverage tests..."
	@./$(BUILD_DIR)/test_full

.PHONY: test-2d-complete
test-2d-complete: $(BUILD_DIR)/test_2d_complete
	@echo "Running 2D complete coverage tests..."
	@./$(BUILD_DIR)/test_2d_complete

$(BUILD_DIR)/fr_test: $(TEST_DIR)/fr_math_test.c $(SRC_DIR)/FR_math.c $(SRC_DIR)/FR_math_2D.cpp
	$(CC) $(CFLAGS) $(TEST_FLAGS) $^ $(LDFLAGS) -lstdc++ -o $@

$(BUILD_DIR)/test_comprehensive: $(TEST_DIR)/test_comprehensive.c $(SRC_DIR)/FR_math.c
	$(CC) $(CFLAGS) $(TEST_FLAGS) $^ $(LDFLAGS) -o $@

$(BUILD_DIR)/test_2d: $(TEST_DIR)/test_2d_math.c $(SRC_DIR)/FR_math.c $(SRC_DIR)/FR_math_2D.cpp
	$(CC) -I$(SRC_DIR) $(LIB_WARN) -Os $(TEST_FLAGS) -c $(SRC_DIR)/FR_math.c -o $(BUILD_DIR)/test_2d_FR_math.o
	$(CXX) $(CXXFLAGS) $(TEST_FLAGS) -c $(SRC_DIR)/FR_math_2D.cpp -o $(BUILD_DIR)/test_2d_FR_math_2D.o
	$(CC) $(CFLAGS) $(TEST_FLAGS) -c $(TEST_DIR)/test_2d_math.c -o $(BUILD_DIR)/test_2d_math.o
	$(CXX) $(TEST_FLAGS) $(BUILD_DIR)/test_2d_math.o $(BUILD_DIR)/test_2d_FR_math.o $(BUILD_DIR)/test_2d_FR_math_2D.o $(LDFLAGS) -o $@

$(BUILD_DIR)/test_overflow: $(TEST_DIR)/test_overflow_saturation.c $(SRC_DIR)/FR_math.c
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

$(BUILD_DIR)/test_full: $(TEST_DIR)/test_full_coverage.c $(SRC_DIR)/FR_math.c
	$(CC) $(CFLAGS) $(TEST_FLAGS) $^ $(LDFLAGS) -o $@

$(BUILD_DIR)/test_2d_complete: $(TEST_DIR)/test_2d_complete.cpp $(SRC_DIR)/FR_math.c $(SRC_DIR)/FR_math_2D.cpp
	$(CC) -I$(SRC_DIR) $(LIB_WARN) -Os $(TEST_FLAGS) -c $(SRC_DIR)/FR_math.c -o $(BUILD_DIR)/test_2dc_FR_math.o
	$(CXX) $(CXXFLAGS) $(TEST_FLAGS) -c $(SRC_DIR)/FR_math_2D.cpp -o $(BUILD_DIR)/test_2dc_FR_math_2D.o
	$(CXX) $(CXXFLAGS) $(TEST_FLAGS) $(TEST_DIR)/test_2d_complete.cpp $(BUILD_DIR)/test_2dc_FR_math.o $(BUILD_DIR)/test_2dc_FR_math_2D.o $(LDFLAGS) -o $@

# Accuracy summary table (extract from test_tdd output)
.PHONY: accuracy accuracy-showpeak
accuracy: dirs $(BUILD_DIR)/test_tdd
	@echo "Running accuracy report..."
	@./$(BUILD_DIR)/test_tdd 2>/dev/null | sed -n '/ACCURACY_TABLE_START/,/ACCURACY_TABLE_END/p'

accuracy-showpeak: dirs $(BUILD_DIR)/test_tdd
	@echo "Running accuracy report (with peak inputs)..."
	@FR_SHOWPEAK=1 ./$(BUILD_DIR)/test_tdd 2>/dev/null | sed -n '/ACCURACY_TABLE_START/,/ACCURACY_TABLE_END/p'

# Coverage report using gcov (no external dependencies)
.PHONY: coverage
coverage:
	@scripts/coverage_report.sh

# Coverage with lcov for HTML reports (requires lcov)
.PHONY: coverage-html
coverage-html: clean dirs
	@echo "Building with coverage flags..."
	@$(CC) -I$(SRC_DIR) $(LIB_WARN) -Os $(TEST_FLAGS) -c $(SRC_DIR)/FR_math.c -o $(BUILD_DIR)/FR_math.o
	@$(CXX) -I$(SRC_DIR) $(LIB_WARN) -Os $(TEST_FLAGS) -c $(SRC_DIR)/FR_math_2D.cpp -o $(BUILD_DIR)/FR_math_2D.o
	@$(CC) $(CFLAGS) $(TEST_FLAGS) $(TEST_DIR)/fr_math_test.c $(BUILD_DIR)/FR_math.o $(BUILD_DIR)/FR_math_2D.o $(LDFLAGS) -lstdc++ -o $(BUILD_DIR)/fr_test
	@echo "Running tests for coverage..."
	@./$(BUILD_DIR)/fr_test
	@command -v lcov >/dev/null 2>&1 || { echo ""; echo "===================================="; echo "lcov is not installed!"; echo ""; echo "To generate HTML coverage reports, install lcov:"; echo "  macOS:  brew install lcov"; echo "  Linux:  sudo apt-get install lcov"; echo "===================================="; echo ""; exit 1; }
	@echo "Generating coverage report..."
	@lcov --directory $(BUILD_DIR) --capture --output-file $(COV_DIR)/coverage.info
	@lcov --remove $(COV_DIR)/coverage.info '/usr/*' '*/tests/*' --output-file $(COV_DIR)/coverage.info
	@lcov --list $(COV_DIR)/coverage.info
	@echo "HTML report: $(COV_DIR)/html/index.html"
	@genhtml $(COV_DIR)/coverage.info --output-directory $(COV_DIR)/html

# Size report - multi-architecture (Docker cross-compilation)
.PHONY: size-report
size-report: dirs
	@scripts/crossbuild_sizes.sh

# Size report + patch doc files
.PHONY: size-update
size-update: dirs
	@scripts/crossbuild_sizes.sh --update

# Simple size report for current platform
.PHONY: size-simple
size-simple: lib
	@echo "=== Build Size Report (Current Platform) ==="
	@echo "Platform: $$(uname -m)"
	@echo ""
	@if command -v size >/dev/null 2>&1; then \
		echo "Object file sizes:"; \
		size $(BUILD_DIR)/*.o; \
	else \
		echo "File sizes:"; \
		ls -lh $(BUILD_DIR)/*.o; \
	fi

# Lean build: only functions with libfixmath equivalents (radian trig,
# inverse trig, sqrt, log2, ln, exp, mul/div — no degree trig, no BAM
# tan, no waves, no hypot exact, no log10).
.PHONY: size-lean
size-lean: dirs
	@echo "=== LEAN Build (FR_LEAN — libfixmath-equivalent API only) ==="
	@$(CC) -I$(SRC_DIR) $(LIB_WARN) -DFR_LEAN -DFR_NO_PRINT -Os -c $(SRC_DIR)/FR_math.c -o $(BUILD_DIR)/FR_math_lean.o
	@size $(BUILD_DIR)/FR_math_lean.o
	@echo ""

# Full build: everything (default — all trig, waves, ADSR, print, etc.)
.PHONY: size-full
size-full: dirs
	@echo "=== FULL Build (all features) ==="
	@$(CC) -I$(SRC_DIR) $(LIB_WARN) -Os -c $(SRC_DIR)/FR_math.c -o $(BUILD_DIR)/FR_math_full.o
	@size $(BUILD_DIR)/FR_math_full.o
	@echo ""

# Side-by-side lean vs full size comparison
.PHONY: size-compare
size-compare: size-lean size-full
	@echo "=== Lean vs Full Comparison ==="
	@LEAN=$$(size $(BUILD_DIR)/FR_math_lean.o | tail -1 | awk '{print $$1}'); \
	 FULL=$$(size $(BUILD_DIR)/FR_math_full.o | tail -1 | awk '{print $$1}'); \
	 echo "  Lean text: $${LEAN} bytes"; \
	 echo "  Full text: $${FULL} bytes"

# Tools
TOOLS_DIR = tools

.PHONY: tools
tools: dirs trig-neighborhood

.PHONY: trig-neighborhood
trig-neighborhood: $(BUILD_DIR)/trig_neighborhood

$(BUILD_DIR)/trig_neighborhood: $(TOOLS_DIR)/trig_neighborhood.cpp $(SRC_DIR)/FR_math.c $(HEADERS)
	$(CC) -I$(SRC_DIR) $(LIB_WARN) -Os -c $(SRC_DIR)/FR_math.c -o $(BUILD_DIR)/tool_FR_math.o
	$(CXX) $(CXXFLAGS) $(TOOLS_DIR)/trig_neighborhood.cpp $(BUILD_DIR)/tool_FR_math.o $(LDFLAGS) -o $@

# Clean
.PHONY: clean
clean:
	rm -rf $(BUILD_DIR) $(COV_DIR)
	rm -f *.o *.gcda *.gcno *.gcov *.exe *.info

.PHONY: cleanall
cleanall: clean
	rm -f *~ $(SRC_DIR)/*~ $(TEST_DIR)/*~

# Basic coverage info without lcov
.PHONY: coverage-basic
coverage-basic: clean dirs
	@echo "Building with coverage flags..."
	@$(CC) -I$(SRC_DIR) $(LIB_WARN) -Os $(TEST_FLAGS) -c $(SRC_DIR)/FR_math.c -o $(BUILD_DIR)/FR_math.o
	@$(CXX) -I$(SRC_DIR) $(LIB_WARN) -Os $(TEST_FLAGS) -c $(SRC_DIR)/FR_math_2D.cpp -o $(BUILD_DIR)/FR_math_2D.o
	@$(CC) $(CFLAGS) $(TEST_FLAGS) $(TEST_DIR)/fr_math_test.c $(BUILD_DIR)/FR_math.o $(BUILD_DIR)/FR_math_2D.o $(LDFLAGS) -lstdc++ -o $(BUILD_DIR)/fr_test
	@echo "Running tests..."
	@./$(BUILD_DIR)/fr_test
	@echo ""
	@echo "=== Basic Coverage Info ==="
	@if command -v gcov >/dev/null 2>&1; then \
		cd $(BUILD_DIR) && gcov FR_math.o | grep -E "File|Lines executed"; \
		echo ""; \
		echo "For detailed coverage report, install lcov and run: make coverage"; \
	else \
		echo "gcov not found. Coverage analysis requires GCC with gcov."; \
	fi
	



