# ============================================================
# Benchmark: FR_math vs libfixmath  (macOS ARM / Apple Clang)
# ============================================================
# WARNING: This Makefile only builds files inside .compare/
#          It does NOT modify anything in the parent repo.
# ============================================================

CXX      := clang++
CC       := clang
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra
CFLAGS   := -std=c11 -O2 -Wall -Wextra

# --- FR_math (parent repo, compiled read-only) ---
FR_SRC   := ../src/FR_math.c
FR_INC   := -I../src

# --- libfixmath (cloned into this dir) ---
LFM_DIR  := libfixmath/libfixmath
LFM_SRC  := $(LFM_DIR)/fix16.c       \
            $(LFM_DIR)/fix16_sqrt.c   \
            $(LFM_DIR)/fix16_exp.c    \
            $(LFM_DIR)/fix16_trig.c   \
            $(LFM_DIR)/fix16_str.c    \
            $(LFM_DIR)/uint32.c       \
            $(LFM_DIR)/fract32.c
LFM_INC  := -I$(LFM_DIR)

# --- Build ---
BUILD    := build
TARGET   := $(BUILD)/benchmark
JSON_OUT := comparison_results.json

OBJS := $(BUILD)/FR_math.o       \
        $(BUILD)/fix16.o          \
        $(BUILD)/fix16_sqrt.o     \
        $(BUILD)/fix16_exp.o      \
        $(BUILD)/fix16_trig.o     \
        $(BUILD)/fix16_str.o      \
        $(BUILD)/uint32.o         \
        $(BUILD)/fract32.o        \
        $(BUILD)/benchmark.o

.PHONY: all clean run size

all: $(TARGET)

run: $(TARGET)
	./$(TARGET) > $(JSON_OUT) 2>comparison_summary.md
	@echo "Results written to $(JSON_OUT) + comparison_summary.md"

# Size comparison: ROM + RAM for both libraries
LFM_OBJS := $(BUILD)/fix16.o $(BUILD)/fix16_sqrt.o $(BUILD)/fix16_exp.o \
            $(BUILD)/fix16_trig.o $(BUILD)/fix16_str.o $(BUILD)/uint32.o \
            $(BUILD)/fract32.o

size: $(BUILD)/FR_math.o $(LFM_OBJS)
	@echo "=== Compiled size: FR_math vs libfixmath ($(CC) -O2) ==="
	@echo ""
	@echo "FR_math (FR_math.o):"
	@size -m $(BUILD)/FR_math.o | grep -E "Section|total"
	@echo ""
	@echo "libfixmath (all objects):"
	@for f in $(LFM_OBJS); do echo "  $$(basename $$f):"; size -m $$f | grep -E "Section" | sed 's/^/    /'; done

$(TARGET): $(OBJS) | $(BUILD)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(BUILD):
	mkdir -p $(BUILD)

# FR_math object
$(BUILD)/FR_math.o: $(FR_SRC) | $(BUILD)
	$(CC) $(CFLAGS) $(FR_INC) -c -o $@ $<

# libfixmath objects
$(BUILD)/fix16.o: $(LFM_DIR)/fix16.c | $(BUILD)
	$(CC) $(CFLAGS) $(LFM_INC) -c -o $@ $<

$(BUILD)/fix16_sqrt.o: $(LFM_DIR)/fix16_sqrt.c | $(BUILD)
	$(CC) $(CFLAGS) $(LFM_INC) -c -o $@ $<

$(BUILD)/fix16_exp.o: $(LFM_DIR)/fix16_exp.c | $(BUILD)
	$(CC) $(CFLAGS) $(LFM_INC) -c -o $@ $<

$(BUILD)/fix16_trig.o: $(LFM_DIR)/fix16_trig.c | $(BUILD)
	$(CC) $(CFLAGS) $(LFM_INC) -c -o $@ $<

$(BUILD)/fix16_str.o: $(LFM_DIR)/fix16_str.c | $(BUILD)
	$(CC) $(CFLAGS) $(LFM_INC) -c -o $@ $<

$(BUILD)/uint32.o: $(LFM_DIR)/uint32.c | $(BUILD)
	$(CC) $(CFLAGS) $(LFM_INC) -c -o $@ $<

$(BUILD)/fract32.o: $(LFM_DIR)/fract32.c | $(BUILD)
	$(CC) $(CFLAGS) $(LFM_INC) -c -o $@ $<

# Benchmark harness
$(BUILD)/benchmark.o: benchmark.cpp | $(BUILD)
	$(CXX) $(CXXFLAGS) $(FR_INC) $(LFM_INC) -c -o $@ $<

clean:
	rm -rf $(BUILD) $(JSON_OUT)
