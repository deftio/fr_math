# log-exp-curves — self-contained build
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

TARGET = log_exp_curves

.PHONY: all run clean

all: $(TARGET)

$(TARGET): log_exp_curves.cpp $(SRC_DIR)/FR_math.c
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) *.o *.gcda *.gcno
