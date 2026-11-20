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

# FR_math.c is C: build it with $(CC), link with $(CXX).
CFLAGS   = -I$(SRC_DIR) -Wall -Wextra -Wshadow -Werror -Os
CXXFLAGS = -I$(SRC_DIR) -Wall -Wextra -Wshadow -Werror -Os
LDFLAGS  = -lm

TARGET = log_exp_curves

.PHONY: all run clean

all: $(TARGET)

$(TARGET): log_exp_curves.cpp FR_math.o
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) -o $@

FR_math.o: $(SRC_DIR)/FR_math.c $(SRC_DIR)/FR_math.h
	$(CC) $(CFLAGS) -c $< -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) *.o *.gcda *.gcno
