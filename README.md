[![License](https://img.shields.io/badge/License-BSD%202--Clause-blue.svg)](https://opensource.org/licenses/BSD-2-Clause)

# FR_Math a C Language Fixed Point Math lib for embedded systems
 
## Overview

FR_Math is a small library provding a small set of fixed-radix functions written in C (and exported to C++ as well) for integer math / fixed radix operations.  

A highlevel list of of operations is here:

* fixed-radix multiply/add/sub with saturation
* trig operations sin,cos, tan and inverse (asin, acos, atan)
* quick interpoloation
* log, log2, log10
* exp, pow2, pow10
* floor / ceil / trun (in fixed radix)
* conversions from degrees / radians/ grads / freq without loss of precision even at high bit packing
* 2D coordinate transformations (rotate / translate / scale between local and global coordinates)
* printing of fixed radix numbers over serial (e.g. print a signed fixed radix number to serial consoles)
* the library has no dependcies and compiles on all C targets with no floating point libraries or emulation needed.

FR_Math allows the computation of fractional quantities with only integer registers but with an eye towards performance rather than just packing floating point equivalents in integer registers.  The implementations here were chosen to minimize the need for overflow tests and similar bounds checking.

This library allows the programmer to choose the radix point (number of fractional bits) for all operations - so one can have 11.4 or 10.5 operations on the fly (where 11 and 10 are left of the radix point and 4 and 5 represent number of fractional bits respectively).  This is useful when using large quantities such occur in frequency domain math operation such as fourier analysis.


## History
I developed this several years ago for use in several embedded interized math projects and this small, slightly cleaned up version is made available for the public here.  It worked very well on 16MHz 68k processors for palm pilots (and later ARM cores) where we needed many integer point transforms for graphics.  This included things like fonting where each letter's M square needed to be computed and embedded graphics transforms.  The Inkstorm application for PalmOS (originally by Trumpetsoft) used a version of this. 

This library supports pure C compilation on x86, ARM, RISC-V, MIPS and other embedded targets.


## Usage

```
#include "FR_defs.h"
#include "FR_math.h"   // includes base fixed radix library
#include "FR_math_2D.h" // optional, this provides the coordinate transform library, if you're not using this then this can be ommited.
//no other dependancies are required.
//compile and link FR_mathroutines.cpp
```

## Introduction to Using Fixed Point Math 

Key ideas that come up in the implementation of quantized or DSP type math areas include how to represent fractional units and what to do when math operations cause integer registers to overflow.  

Floating point (float, or double types in C,C++) are most often used to represent fractional units but these are not always available (no hardware support or on low end microcontrollers no library software support).   If hardware support is not available then floating point can be provided via a software library, however these libraries are generally atleast an order of magnitude slower than hardware implementations.  If the programmer is clever, integer math can be used instead of software floating point resulting in much faster code.  In these cases fractional units can be represented with integer registers (short, int, long) if a scaling factor is used.  


### Scaling and Wrap Around

Two common issues that come up in creating scaled-integer representations of numbers are

* Scaling - The programmer must keep track of the fractional scaling factor by hand whereas when using floating point numbers the compiler and floating point library do this for the programmer.

* Overflow and Wrap Around - if two large numbers are added the result may be larger than can fit in a integer representation, resulting in wrap-around or overflow errors.  Worse yet these errors may go unnoticed depending on compiler warning settings or types of operations.  For example take two 8 bit numbers (usually a char in C/C++) 

```
char a = 34, b= 3, c;  

//and compute 

c=a*b;

//Multiply them together and the result is 102, which still is fits in a 8 bit result.  But
//what happens if b = 5?

c=a*b; // the real answer is 170, but the result will be -86

```

This type of error called an overflow or wrap around error and can be dealt with in several ways.  We could use larger integer types such as shorts, or ints, or we could test the numbers beforehand to see if the results would produce an overflow.  Which is better?  It depends on the circumstances.  If we are already using the largest natural type (such as a 32 bit integer on a 32 bit CPU) then we may have to test the value before the operation is performed even if this incurs some runtime performance penalty. 

### Loss of Precision

True floating point representations can maintain a _relatively_ arbitrary set of precision across a range of operations, however when using fixed point (scaled) math the process of scaling means we end up dedicating a portion of the available bits to the precision we desire.  This means that mathematical information that is smaller than the scaling factor will be lost causing quantization error.


A Simple Naive Scaling Example
Lets try to represent the number 10.25 with an integer we could do something 
simple like multiply the the value by 100 and store the result in a integer variable.
so we have:

10.25 * 100 ==> 1025

If we wanted to add another number, say 0.55 to it
we would take 1.55 and scale it too up by 100.  so 

0.55 * 100  ==>  55

Now to add them together we add the integerized numbers

1025+55     ==> 1070

But lets look at this with some code:

```
void main (void)
{
	int myIntegerizedNumber  = 1025;
	int myIntegerizedNumber2 =  55;
	int myIntegerizedNumber3;

	myIntegerizedNumber3 = myIntegerizedNumber1 + myIntegerizedNumber2;

	printf("%d + %d = %d\n",myIntegerizedNumber1,myIntegerizedNumber2,myIntegerizedNumber3);
}

```

But now comes the first of a few challenges.  How do we deal with the integer and fractional parts?  How do we display the results?  There is no compiler support, we as the programmer must separate the integer and fractional results.  The program above prints the result as 1070 not 10.70 because the compiler only knows about the integer variables we have used not our intended scaled up definitions.

### Thinking in powers of 2 (radixes)

In the previous example we used base10 math, which while useful for humans is not an optimal use of bits as all the numerics in the machine will be using binary math.  If we use powers of two we can specify the precision in terms of bits instead of base10 for the fractional and integer parts and we get several other advantages:

1. Ease of notation - for example with a 16 bit signed integer (typically a short in C/C++), we can say the number is "s11.4"  which means its a number that is signed with 11 bits of integer and 4 bits of fractional precision.  In fact one bit is not used for a sign representation but the number is represented as 2's complement format.  However _effectively_ 1 bit is used for sign representation from the point of precision represented.  If a number is unsigned, then we can say its u12.4 - yes the same number now has 12 integer bits of precision and 4 bits of fractional representation.  

If we were to use base10 math no such simple mapping would be possible (I won't go in to all the base10 issues that will come up).  Worse yet many divide by 10 operations would need to be performed which are slow and can result in precision loss.

2. Ease of changing radix precision.  Using base2 radixes allows us to use simple shifts (<< and >>) to change from integer to fixed-point or change from different fixed point representations.  Many programmers assume that radixes should be a byte multiple like 16bits or 8 bits but actually using just enough precision and no more (like 4 or 5 bits for small graphics apps) can allow much larger headroom because integer portion will get the remaining bits.  Suppose we need a range of +/-800 and a precision of 0.05 (or 1/20 in decimal).  We can fit this in to 16bit integer as follows.  First one bit is allocated to sign.  This leaves 15 bits of resolution.  Now we need 800 counts of range.  Log2(800) = 9.64..  So we need 10 bits for the integer dynamic range.  Now lets look at the fractional precision, we need log2(1/(0.05))= 4.32 bits which rounded up is 5 bits.  So we could, in this application use a fixed radix of s10.5 or signed 10bit integer and 5 bit fractional resolution.  Better yet it still fits in a 16 bit integer.  Now there are some issues:  while the fractional precision is 5 bits (or 1/32 or about 0.03125) which is better than the requirement of 0.05, it is not identical and so with accumulated operations we will get quantization errors.   This can be greatly reduced by moving to a larger integer and radix (e.g. 32bit integer with more fractional bits) but often this is not necessary and manipulating 16 bit integers is much more efficient in both compute and memory for smaller processors.  The choice of these integer sizes etc should be made with care.

### A few notes on fixed point precision

When adding fixed point numbers together its important to align their radix points (e.g. you must add 12.4 and 12.4 numbers or even 18.4 + 12.4 + 24.4 will work where the integer portion represents the number of *bits in use* not the physical size of the integer register declared).  Now, and here begins some of the trickyiness, the result, purely speaking is a 13.4 number!  But this becomes 17 bits - so you must be careful to watch for overflow.  One way is to put the results in a larger 32 bit wide register of 28.4 precision.  Another way is to test and see if the high order bits of either of the operands is actually set - if not then the numbers can be safely added despite the register width precision limitations.  Another solution is to use saturating math - if the result is larger than can be put in the register we set the result to the maximum possible value.  Also be sure to watch out for sign.  Adding two negative numbers can cause wrap around just as easy as two positive numbers.

An interesting problem that comes up in designing fixed radix pipelines is keeping track of how much of the available precision is actually in use. While you may be using  This is especially true for operations like Fourier transforms and the like which can cause some array cells to have relatively large values while others have near zero mathematical energy.

Some rules:
Adding 2 M bit numbers results in a M+1 bit precision result (without testing for overflow)
Adding N M bit numbers results in M + log2(N) bit precision result (without testing for overflow)

Multiply an M bit number by a N bit number results in a N+M bit precision result (without testing for overflow)

Saturation can be useful in some circumstance but may result in performance hits or loss of data.

#### Adding...

When adding or subtracting fixed radix numbers the radix points must be aligned beforehand.  For example: to add a A is a s11.4 number and B is a 9.6 number.  We need to make some choices.  We could move them to larger registers first, say 32 bit registers. resulting in A2 being a s27.4 number and B2 being a s25.6 number.  Now we can safely shift A2 up by two bits so that A2 = A2<<2.  Now A2 is a s25.6 number but we haven't lost any data because the upper bits of the former A can be held in the larger register without precision loss.  

Now we can add them and get the result C=A2+B2.  The result is a s25.6 number but the precision is actually 12.6 (we use the larger integer portion which comes from A which is 11 bits and the larger fractional portion, which comes from B which is 6 bits, plus 1 bit from addition operation).  So this s12.6 number has 18+1 bits of precision without any accuracy loss.  But if we need to convert it back to a 16 bit precision number we will need to make choices as to how much fractional accuracy to keep. The simplest way is to preserve all the integer bits so we shift C down by enough bits that the sign and integer bits fit in a 16 bit register.  So C=C>>3 results in a s12.3 number.  As long as we keep track of the radix point we will have maintained accuracy.  Now if we had tested A and B before hand we may have learned that we could keep more of the fractional bits.

#### Multiplying...

Multiplying does not require that the radix points be aligned before the operation is carried out.  Lets assume we have two numbers as we had in our Adding example.  A is a s11.4 precision number which we move to A2 now a s27.4 large register (but still a s11.4 number of bits are in use). B is a s9.6 number which we move to B2 now a s25.6 large registor (but still a s9.6 number of bits are in use).  C=A2*B2 results in C being a s20.10 number.  note that C *is* using the entire 32 bit register for the result.  Now if we want the result squished back in to a 16 bit register then we must make some hard choices.  Firstly we already have 20 bits if integer precision - so any attempt (without looking at the number of bits actually used) to fit the result in to a 16 bit regist must result in some type of truncation.  If we take the top 15 bits of the result (+1 bit for sign precision) we the programmers must remember that this scaled up by 20-5 = 5 bits of precision.  So when even though we can fit the top 15 bits in a 16 bit register we will lose the bottom 16 bits of precision and we have to remember that the result is scaled by 5 bits (or integer 32).  Interestingly if test both A and B beforehand we may find that while they had the stated incoming precion by wrote they may not actually contain that number of live, set bits (e.g. if A is a s11.4 number by programmer's convention but its actual value is integer 33 or fixed-radix 2and1/16 then we may not have as many bits to truncate in C).

## Conventions Used in the Library

This library uses radix points (binary points) instead of base10 math, so for those who are not used to thinking in powers of two I suggest glancing at the FR_main code which contains a few examples of its use.

Mixed radix types for matrices and numbers is allowed and encouraged but monitoring of mixed radix precisionis left to the user of the functions.

Also all types are type-def'd to represent the amount of bits available and the sign.
s8 	= signed 8  bit integer
s16 = signed 16 bit integer
s32 = signed 32 bit integer
u8	= unsigned 8 bit integer
u16	= unsigned 16 bit integer
u32	= unsigned 32 bit integer

## This Library...

This library just includes a handful of functions for mixed integer radix math and for 2D coordinate transforms using fixed radix math.  this is especially useful on low end embedded systems or systems without floating point or SIMD support which need simple graphical transforms such as scale, rotate, skew, translate of both 16 or 32 bit integer coordinates.  The 2D matrix also allows for forward or backward transforms of points (e.g. camera to world and world to camera) transformations.

Much of the library is given as MACROS (which always appear in UPPER CASE and always with the prefix FR_).  The reason for this is it keeps compiled library size down which can then be used as shared object rather than depending on an incremental linker.   However if some of the macros are used a lot it may be desirable to wrap them in a function call to trade inline expansion for code space reduction.

eg. the following wraps the macro FR_ADD() in to a function to save code space.
fr_add(s32 x, int xr, s32 y, int yr)
{
	return FR_ADD(x,xr,y,yr);
}

## Finally ... A few thoughts on higher end CPUs, and embedded processors, GPUs, and DSPs:

DSP (Digital Signal Processors) have built operations for saturating math.  Usually this is done by providing CPU instructions which have both traditional 2's complement and saturating math. e.g. The low level programmer can determine when to operands are multiplied *which* kind of multiply to use saturated or unsaturated.  This also applies to addition, subtraction and combined multiply-accumulate instructions.  Also modern CPUs (80x86, x64, ARM, Power series and DSPs) often include parallel instruction sets (SIMD).  Examples include MMX, SSE, Altivec, NEON etc.  These instruction sets allow block-loads of data in to special registers which can execute multiple math operations in parallel.  For example in MMX or Altivec we can use  128 bit registers each of which can hold four 32 bit integers.  So now when we multiply 2 of these registers together we get 4 simultaneous multiplies.  Even better the instructions are flexible in how they the internal data of the 128 bit integers and can treat a 128 bit register as 4 separate numbers and the programmer can choose the saturating / nonsaturating behavior.  Most SIMD instruction sets allow the programmer to select the packing (four 32bit numbers or eight 16bit numbers or sixteen 8 bit numbers and even packed floating point) and signed/unsigned/saturated operations.  Also similar is GPU or tensor computing hardware which have usually been optimized for linear algebra / packed data operations.  In these systems packed data is also operated on en-masse to provide high performance vector computes.

More recent instruction sets combine advanced cryptographic, image processing, and transcendental functions for use in DSP like media processing.

These powerful instruction sets can speed up repetive matrix oriented operations literally an order of magnitude or more compared with the regular CPU instruction stream as long as the data is already acked in the correct format.

Thanks for taking a look!

Cheers and I hope you find this useful.  
Feedback and bugs are welcome at the email address listed at the top.

## LICENSE

Licensed under BSD-2 License (see LICENSE.TXT included)
This library can be used freely in open source or commercial projects.  
(c) 2000-2023  M. A. Chatterjee 

-Manjirnath (Manu) Chatterjee

