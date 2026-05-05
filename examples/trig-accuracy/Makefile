# trig-accuracy — self-contained build
# All artifacts stay in this directory.
#
# Requires libfixmath source at ../../compare_lfm/libfixmath/libfixmath/
#
# Usage:
#   make          Build the example (fails if libfixmath not found)
#   make run      Build and run
#   make clean    Remove build artifacts

CC  ?= gcc
CXX ?= g++
SRC_DIR = ../../src
LFM_DIR = ../../compare_lfm/libfixmath/libfixmath

CXXFLAGS = -I$(SRC_DIR) -I$(LFM_DIR) -Wall -Wextra -Wshadow -Os
CFLAGS   = -I$(SRC_DIR) -I$(LFM_DIR) -Wall -Wextra -Wshadow -Os
LDFLAGS  = -lm

TARGET = trig_accuracy

# libfixmath .c sources needed for sin/cos/tan
LFM_SRCS = $(LFM_DIR)/fix16.c $(LFM_DIR)/fix16_trig.c $(LFM_DIR)/fix16_sqrt.c $(LFM_DIR)/fix16_exp.c
LFM_OBJS = fix16.o fix16_trig.o fix16_sqrt.o fix16_exp.o

.PHONY: all run clean check-lfm

all: check-lfm $(TARGET)

check-lfm:
	@if [ ! -f $(LFM_DIR)/fix16.h ]; then \
		echo "ERROR: libfixmath not found at $(LFM_DIR)"; \
		echo "This example requires libfixmath source."; \
		echo "Clone it into compare_lfm/libfixmath/ from the repo root."; \
		exit 1; \
	fi

fix16.o: $(LFM_DIR)/fix16.c
	$(CC) $(CFLAGS) -c $< -o $@

fix16_trig.o: $(LFM_DIR)/fix16_trig.c
	$(CC) $(CFLAGS) -c $< -o $@

fix16_sqrt.o: $(LFM_DIR)/fix16_sqrt.c
	$(CC) $(CFLAGS) -c $< -o $@

fix16_exp.o: $(LFM_DIR)/fix16_exp.c
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): trig_accuracy.cpp $(SRC_DIR)/FR_math.c $(LFM_OBJS)
	$(CXX) $(CXXFLAGS) trig_accuracy.cpp $(SRC_DIR)/FR_math.c $(LFM_OBJS) $(LDFLAGS) -o $@

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) *.o *.gcda *.gcno
