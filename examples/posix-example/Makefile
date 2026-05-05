# posix-example — self-contained build
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

TARGET = FR_Math_Example1

.PHONY: all run clean

all: $(TARGET)

$(TARGET): FR_Math_Example1.cpp $(SRC_DIR)/FR_math.c $(SRC_DIR)/FR_math_2D.cpp
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) *.o *.gcda *.gcno
