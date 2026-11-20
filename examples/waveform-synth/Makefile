# waveform-synth — self-contained build
# All artifacts stay in this directory.
#
# Usage:
#   make          Build the example
#   make run      Build and run (ASCII art mode)
#   make run-csv  Build and run (CSV output)
#   make clean    Remove build artifacts

CC  ?= gcc
CXX ?= g++
SRC_DIR = ../../src

# Stores sine in s16 via (s16)(fr_sin_bam(phase) >> 1), so build with
# FR_TRIG_S16_SAFE (crest 65534) to avoid the exact-crest wrap at ±1.0.
# The flag must reach FR_math.c (the table lives there). FR_math.c is C:
# build it with $(CC), link with $(CXX).
CFLAGS   = -I$(SRC_DIR) -Wall -Wextra -Wshadow -Werror -Os -DFR_TRIG_S16_SAFE
CXXFLAGS = -I$(SRC_DIR) -Wall -Wextra -Wshadow -Werror -Os -DFR_TRIG_S16_SAFE
LDFLAGS  = -lm

TARGET = waveform_synth

.PHONY: all run run-csv clean

all: $(TARGET)

$(TARGET): waveform_synth.cpp FR_math.o
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) -o $@

FR_math.o: $(SRC_DIR)/FR_math.c $(SRC_DIR)/FR_math.h
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

run-csv: $(TARGET)
	./$(TARGET) --csv

clean:
	rm -f $(TARGET) *.o *.gcda *.gcno
