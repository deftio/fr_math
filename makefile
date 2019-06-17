# make file for FR_math  ==> test function set
# @author M A Chatterjee <deftio [at] deftio [dot] com>

CC=gcc
CFLAGS=-I. -Wall -Os -ftest-coverage -fprofile-arcs 
DEPS = FR_math.h FR_defs.h FR_math_2D.h

                  
%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

OBJ = FR_math.o FR_math_2D.o FR_Math_Example1.o 
FR_math_example1.exe: $(OBJ)
	gcc -o $@ $^ $(CFLAGS) -lm -lstdc++


OBJTEST = FR_math_test.o FR_math_2D.o fr_math_test.o 
FR_math_test.exe: $(OBJTEST)
	gcc -o $@ $^ $(CFLAGS) -lm  -Os 

clean :
	rm  *.o  *.asm  *.lst *.sym *.rel *.s *.gc* *.exe -f *.info

cleanall :
	rm *.o *~
	



