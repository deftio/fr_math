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

CXXFLAGS = -I$(SRC_DIR) -Wall -Wextra -Wshadow -Os
LDFLAGS  = -lm

TARGET = waveform_synth

.PHONY: all run run-csv clean

all: $(TARGET)

$(TARGET): waveform_synth.cpp $(SRC_DIR)/FR_math.c
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) -o $@

run: $(TARGET)
	./$(TARGET)

run-csv: $(TARGET)
	./$(TARGET) --csv

clean:
	rm -f $(TARGET) *.o *.gcda *.gcno
