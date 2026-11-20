# trig-crest — two builds of the same demo
#   trig_crest_exact      default exact ±1.0 crest (shows s16 wrap)
#   trig_crest_s16_safe   -DFR_TRIG_S16_SAFE (safe cast)
#
# Usage:
#   make          Build both programs
#   make run      Build and run both
#   make clean    Remove build artifacts

CC  ?= gcc
CXX ?= g++
SRC_DIR = ../../src

# FR_math.c is C: build it with $(CC) (once per crest mode), link with $(CXX).
CFLAGS   = -I$(SRC_DIR) -Wall -Wextra -Wshadow -Werror -Os
CXXFLAGS = -I$(SRC_DIR) -Wall -Wextra -Wshadow -Werror -Os
LDFLAGS  = -lm

.PHONY: all run clean

all: trig_crest_exact trig_crest_s16_safe

trig_crest_exact: trig_crest.cpp FR_math_exact.o
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) -o $@

trig_crest_s16_safe: trig_crest.cpp FR_math_s16_safe.o
	$(CXX) $(CXXFLAGS) -DFR_TRIG_S16_SAFE $^ $(LDFLAGS) -o $@

FR_math_exact.o: $(SRC_DIR)/FR_math.c $(SRC_DIR)/FR_math.h
	$(CC) $(CFLAGS) -c $< -o $@

FR_math_s16_safe.o: $(SRC_DIR)/FR_math.c $(SRC_DIR)/FR_math.h
	$(CC) $(CFLAGS) -DFR_TRIG_S16_SAFE -c $< -o $@

run: all
	@echo "=== trig_crest_exact ==="
	./trig_crest_exact
	@echo ""
	@echo "=== trig_crest_s16_safe ==="
	./trig_crest_s16_safe

clean:
	rm -f trig_crest_exact trig_crest_s16_safe *.o *.gcda *.gcno
