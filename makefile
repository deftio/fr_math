# make file for FR_math test function set
# @author M A Chatterjee <deftio [at] deftio [dot] com>

CC=gcc
CFLAGS=-I. -Wall -Os 
DEPS = FR_mathroutines.h FR_defs.h
OBJ = FR_math.o FR_math_2D.o FR_main.o  

                  
%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

FR_main: $(OBJ)
	gcc -o $@ $^ $(CFLAGS) -lm -lstdc++



clean :
	rm  *.o 

cleanall :
	rm *.o *~
	

