# fixed-point-basics — self-contained build
# All artifacts stay in this directory.
#
# Usage:
#   make          Build the example
#   make run      Build and run
#   make clean    Remove build artifacts

CC  ?= gcc
CXX ?= g++
SRC_DIR = ../../src

CXXFLAGS = -I$(SRC_DIR) -Wall -Wextra -Wshadow -Os
LDFLAGS  = -lm

TARGET = fixed_point_basics

.PHONY: all run clean

all: $(TARGET)

$(TARGET): fixed_point_basics.cpp $(SRC_DIR)/FR_math.c
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) *.o *.gcda *.gcno
