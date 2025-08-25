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

# Compiler flags
CFLAGS = -I$(SRC_DIR) -Wall -Os
CXXFLAGS = $(CFLAGS)
TEST_FLAGS = -ftest-coverage -fprofile-arcs
LDFLAGS = -lm

# Source files
HEADERS = $(SRC_DIR)/FR_defs.h $(SRC_DIR)/FR_math.h $(SRC_DIR)/FR_math_2D.h

# Default target
.PHONY: all
all: dirs lib examples

# Create build directories
.PHONY: dirs
dirs:
	@mkdir -p $(BUILD_DIR) $(COV_DIR)

# Build library
.PHONY: lib
lib: dirs $(BUILD_DIR)/FR_math.o $(BUILD_DIR)/FR_math_2D.o

$(BUILD_DIR)/FR_math.o: $(SRC_DIR)/FR_math.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/FR_math_2D.o: $(SRC_DIR)/FR_math_2D.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Build examples
.PHONY: examples
examples: dirs $(BUILD_DIR)/fr_example

$(BUILD_DIR)/fr_example: $(EXAMPLE_DIR)/FR_Math_Example1.cpp $(BUILD_DIR)/FR_math.o $(BUILD_DIR)/FR_math_2D.o
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) -o $@

# Build and run tests
.PHONY: test
test: dirs test-basic test-comprehensive test-2d test-overflow test-full test-2d-complete

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
	$(CXX) $(CXXFLAGS) $(TEST_FLAGS) $^ $(LDFLAGS) -o $@

$(BUILD_DIR)/test_overflow: $(TEST_DIR)/test_overflow_saturation.c $(SRC_DIR)/FR_math.c
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

$(BUILD_DIR)/test_full: $(TEST_DIR)/test_full_coverage.c $(SRC_DIR)/FR_math.c
	$(CC) $(CFLAGS) $(TEST_FLAGS) $^ $(LDFLAGS) -o $@

$(BUILD_DIR)/test_2d_complete: $(TEST_DIR)/test_2d_complete.cpp $(SRC_DIR)/FR_math.c $(SRC_DIR)/FR_math_2D.cpp
	$(CXX) $(CXXFLAGS) $(TEST_FLAGS) $^ $(LDFLAGS) -o $@

# Coverage report using gcov (no external dependencies)
.PHONY: coverage
coverage:
	@scripts/coverage_report.sh

# Coverage with lcov for HTML reports (requires lcov)
.PHONY: coverage-html
coverage-html: clean dirs
	@echo "Building with coverage flags..."
	@$(CC) $(CFLAGS) $(TEST_FLAGS) -c $(SRC_DIR)/FR_math.c -o $(BUILD_DIR)/FR_math.o
	@$(CXX) $(CXXFLAGS) $(TEST_FLAGS) -c $(SRC_DIR)/FR_math_2D.cpp -o $(BUILD_DIR)/FR_math_2D.o
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

# Size report - multi-architecture
.PHONY: size-report
size-report: dirs
	@scripts/size_report.sh

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

# Clean
.PHONY: clean
clean:
	rm -rf $(BUILD_DIR) $(COV_DIR)
	rm -f *.o *.gcda *.gcno *.exe *.info

.PHONY: cleanall
cleanall: clean
	rm -f *~ $(SRC_DIR)/*~ $(TEST_DIR)/*~

# Basic coverage info without lcov
.PHONY: coverage-basic
coverage-basic: clean dirs
	@echo "Building with coverage flags..."
	@$(CC) $(CFLAGS) $(TEST_FLAGS) -c $(SRC_DIR)/FR_math.c -o $(BUILD_DIR)/FR_math.o
	@$(CXX) $(CXXFLAGS) $(TEST_FLAGS) -c $(SRC_DIR)/FR_math_2D.cpp -o $(BUILD_DIR)/FR_math_2D.o
	@$(CC) $(CFLAGS) $(TEST_FLAGS) $(TEST_DIR)/fr_math_test.c $(BUILD_DIR)/FR_math.o $(BUILD_DIR)/FR_math_2D.o $(LDFLAGS) -lstdc++ -o $(BUILD_DIR)/fr_test
	@echo "Running tests..."
	@./$(BUILD_DIR)/fr_test
	@echo ""
	@echo "=== Basic Coverage Info ==="
	@if command -v gcov >/dev/null 2>&1; then \
		gcov $(SRC_DIR)/FR_math.c -o $(BUILD_DIR) | grep -E "File|Lines executed"; \
		echo ""; \
		echo "For detailed coverage report, install lcov and run: make coverage"; \
	else \
		echo "gcov not found. Coverage analysis requires GCC with gcov."; \
	fi
	



