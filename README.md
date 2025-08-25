[![License](https://img.shields.io/badge/License-BSD%202--Clause-blue.svg)](https://opensource.org/licenses/BSD-2-Clause)
[![CI](https://github.com/deftio/fr_math/actions/workflows/ci.yml/badge.svg)](https://github.com/deftio/fr_math/actions/workflows/ci.yml)

# FR_Math: A C Language Fixed-Point Math Library for Embedded Systems
 
## Overview

FR_Math is a small library providing a comprehensive set of fixed-radix / mixed-radix functions written in C (and exported to C++ as well) for integer math arithmetic.  

A high-level list of operations includes:

* Fixed-radix multiply/add/sub with saturation
* Trigonometric operations: sin, cos, tan and inverses (asin, acos, atan)
* Quick interpolation
* Logarithms: log, log2, log10
* Exponentials: exp, pow2, pow10
* Floor/ceil/truncate (in fixed radix)
* Conversions between degrees/radians/grads/freq without loss of precision even at high bit packing
* 2D coordinate transformations (rotate/translate/scale between local and global coordinates)
* Printing of fixed-radix numbers over serial (e.g., print a signed fixed-radix number to serial consoles)
* The library has no dependencies and compiles on all C targets with no floating-point libraries or emulation needed

FR_Math allows the computation of fractional quantities with only integer registers but with an eye towards performance rather than just packing floating-point equivalents in integer registers. The implementations here were chosen to minimize the need for overflow tests and similar bounds checking.

This library allows the programmer to choose the radix point (number of fractional bits) for all operations—so one can have 11.4 or 10.5 operations on the fly (where 11 and 10 are left of the radix point and 4 and 5 represent the number of fractional bits respectively). This is useful when using large quantities such as those that occur in frequency domain math operations like Fourier analysis.  All operations are performed in integer registers using pure C (with exports to C++).  Integer registers from 16 / 32 / 64 bit widths are supported but the focus is on 32 bit registers.

## History
I developed this several years ago for use in several embedded integer-only math projects, and this small, slightly cleaned-up version is made available for the public here. It worked very well on 16MHz 68k processors for Palm Pilots (and later ARM cores) where we needed many integer point transforms for graphics. This included things like font rendering where each letter's M-square needed to be computed and embedded graphics transforms. The Inkstorm application for PalmOS (originally by Trumpetsoft) used a version of this.

This library supports pure C compilation on x86, ARM, RISC-V, MIPS, ESP32, 68k and other embedded targets.


## Building and Testing

### Quick Start

```bash
# Clone the repository
git clone https://github.com/deftio/fr_math.git
cd fr_math

# Build the library
make lib

# Run all tests
make test

# Build examples
make examples

# Generate coverage report (requires lcov)
make coverage
```

### Build Targets

- `make lib` - Build the static library
- `make test` - Build and run all tests
- `make examples` - Build example programs
- `make coverage` - Generate code coverage report
- `make clean` - Clean build artifacts

The library has been tested on:
- x86/x64 (Linux, macOS)
- ARM (32-bit and 64-bit)
- RISC-V
- 32-bit targets

## Usage

```c
#include "FR_defs.h"
#include "FR_math.h"    // Includes base fixed-radix library
#include "FR_math_2D.h"  // Optional: provides coordinate transform library (omit if not needed)
// No other dependencies are required
// Compile and link FR_math.c and FR_math_2D.cpp
```

## Introduction to Using Fixed-Point Math 

Key ideas that come up in the implementation of quantized or DSP-type math areas include how to represent fractional units and what to do when math operations cause integer registers to overflow.

Floating point (float or double types in C/C++) are most often used to represent fractional units, but these are not always available (no hardware support or, on low-end microcontrollers, no library software support). If hardware support is not available, then floating point can be provided via a software library; however, these libraries are generally at least an order of magnitude slower than hardware implementations. If the programmer is clever, integer math can be used instead of software floating point, resulting in much faster code. In these cases, fractional units can be represented with integer registers (short, int, long) if a scaling factor is used.  


### Scaling and Wrap-Around

Two common issues that come up in creating scaled-integer representations of numbers are:

* **Scaling** - The programmer must keep track of the fractional scaling factor by hand, whereas when using floating-point numbers, the compiler and floating-point library do this for the programmer.

* **Overflow and Wrap-Around** - If two large numbers are added, the result may be larger than can fit in an integer representation, resulting in wrap-around or overflow errors. Worse yet, these errors may go unnoticed depending on compiler warning settings or types of operations. For example, take two 8-bit numbers (usually a char in C/C++):

```c
char a = 34, b = 3, c;  

// Compute:
c = a * b;

// Multiply them together and the result is 102, which still fits in an 8-bit result.  
// But what happens if b = 5?

c = a * b; // The real answer is 170, but the result will be -86
```

This type of error, called an overflow or wrap-around error, can be dealt with in several ways. We could use larger integer types such as shorts or ints, or we could test the numbers beforehand to see if the results would produce an overflow. Which is better? It depends on the circumstances. If we are already using the largest natural type (such as a 32-bit integer on a 32-bit CPU), then we may have to test the value before the operation is performed, even if this incurs some runtime performance penalty. 

### Loss of Precision

True floating-point representations can maintain a _relatively_ arbitrary set of precision across a range of operations; however, when using fixed-point (scaled) math, the process of scaling means we end up dedicating a portion of the available bits to the precision we desire. This means that mathematical information that is smaller than the scaling factor will be lost, causing quantization error.

#### A Simple Naive Scaling Example
Let's try to represent the number 10.25 with an integer. We could do something simple like multiply the value by 100 and store the result in an integer variable:

10.25 * 100 → 1025

If we wanted to add another number, say 0.55 to it, we would take 0.55 and scale it up by 100:

0.55 * 100 → 55

Now to add them together, we add the integerized numbers:

1025 + 55 → 1070

But let's look at this with some code:

```c
void main(void)
{
    int myIntegerizedNumber1 = 1025;
    int myIntegerizedNumber2 = 55;
    int myIntegerizedNumber3;

    myIntegerizedNumber3 = myIntegerizedNumber1 + myIntegerizedNumber2;

    printf("%d + %d = %d\n", myIntegerizedNumber1, myIntegerizedNumber2, myIntegerizedNumber3);
}
```

But now comes the first of a few challenges. How do we deal with the integer and fractional parts? How do we display the results? There is no compiler support; we as the programmer must separate the integer and fractional results. The program above prints the result as 1070, not 10.70, because the compiler only knows about the integer variables we have used, not our intended scaled-up definitions.

### Thinking in Powers of 2 (Radixes)

In the previous example, we used base-10 math, which, while useful for humans, is not an optimal use of bits as all the numerics in the machine will be using binary math. If we use powers of two, we can specify the precision in terms of bits instead of base-10 for the fractional and integer parts, and we get several other advantages:

1. **Ease of notation** - For example, with a 16-bit signed integer (typically a short in C/C++), we can say the number is "s11.4", which means it's a number that is signed with 11 bits of integer and 4 bits of fractional precision. In fact, one bit is not used for a sign representation, but the number is represented in 2's complement format. However, _effectively_ 1 bit is used for sign representation from the point of precision represented. If a number is unsigned, then we can say it's u12.4—yes, the same number now has 12 integer bits of precision and 4 bits of fractional representation.  

If we were to use base-10 math, no such simple mapping would be possible (I won't go into all the base-10 issues that will come up). Worse yet, many divide-by-10 operations would need to be performed, which are slow and can result in precision loss.

2. **Ease of changing radix precision** - Using base-2 radixes allows us to use simple shifts (`<<` and `>>`) to change from integer to fixed-point or change between different fixed-point representations. Many programmers assume that radixes should be a byte multiple like 16 bits or 8 bits, but actually using just enough precision and no more (like 4 or 5 bits for small graphics apps) can allow much larger headroom because the integer portion will get the remaining bits. 

   Suppose we need a range of ±800 and a precision of 0.05 (or 1/20 in decimal). We can fit this into a 16-bit integer as follows: First, one bit is allocated to sign. This leaves 15 bits of resolution. Now we need 800 counts of range. Log₂(800) = 9.64... So we need 10 bits for the integer dynamic range. Now let's look at the fractional precision: we need log₂(1/0.05) = 4.32 bits, which rounded up is 5 bits. So we could, in this application, use a fixed radix of s10.5 or signed 10-bit integer and 5-bit fractional resolution. Better yet, it still fits in a 16-bit integer. 
   
   Now there are some issues: while the fractional precision is 5 bits (or 1/32 or about 0.03125), which is better than the requirement of 0.05, it is not identical, and so with accumulated operations we will get quantization errors. This can be greatly reduced by moving to a larger integer and radix (e.g., 32-bit integer with more fractional bits), but often this is not necessary, and manipulating 16-bit integers is much more efficient in both compute and memory for smaller processors. The choice of these integer sizes should be made with care.

### A Few Notes on Fixed-Point Precision

When adding fixed-point numbers together, it's important to align their radix points (e.g., you must add 12.4 and 12.4 numbers, or even 18.4 + 12.4 + 24.4 will work, where the integer portion represents the number of *bits in use*, not the physical size of the integer register declared). Now, and here begins some of the trickiness, the result, purely speaking, is a 13.4 number! But this becomes 17 bits—so you must be careful to watch for overflow. One way is to put the results in a larger 32-bit wide register of 28.4 precision. Another way is to test and see if the high-order bits of either of the operands are actually set—if not, then the numbers can be safely added despite the register width precision limitations. Another solution is to use saturating math—if the result is larger than can be put in the register, we set the result to the maximum possible value. Also be sure to watch out for sign. Adding two negative numbers can cause wrap-around just as easily as two positive numbers.

An interesting problem that comes up in designing fixed-radix pipelines is keeping track of how much of the available precision is actually in use. This is especially true for operations like Fourier transforms and the like, which can cause some array cells to have relatively large values while others have near-zero mathematical energy.

#### Some Rules:
- Adding 2 M-bit numbers results in an (M+1)-bit precision result (without testing for overflow)
- Adding N M-bit numbers results in an M + log₂(N)-bit precision result (without testing for overflow)
- Multiplying an M-bit number by an N-bit number results in an (N+M)-bit precision result (without testing for overflow)

Saturation can be useful in some circumstances but may result in performance hits or loss of data.

#### Adding

When adding or subtracting fixed-radix numbers, the radix points must be aligned beforehand. For example: to add A (an s11.4 number) and B (a 9.6 number), we need to make some choices. We could move them to larger registers first, say 32-bit registers, resulting in A2 being an s27.4 number and B2 being an s25.6 number. Now we can safely shift A2 up by two bits so that A2 = A2<<2. Now A2 is an s25.6 number, but we haven't lost any data because the upper bits of the former A can be held in the larger register without precision loss.  

Now we can add them and get the result C = A2 + B2. The result is an s25.6 number, but the precision is actually 12.6 (we use the larger integer portion, which comes from A at 11 bits, and the larger fractional portion, which comes from B at 6 bits, plus 1 bit from the addition operation). So this s12.6 number has 18+1 bits of precision without any accuracy loss. But if we need to convert it back to a 16-bit precision number, we will need to make choices as to how much fractional accuracy to keep. The simplest way is to preserve all the integer bits, so we shift C down by enough bits that the sign and integer bits fit in a 16-bit register. So C = C>>3 results in an s12.3 number. As long as we keep track of the radix point, we will have maintained accuracy. Now, if we had tested A and B beforehand, we may have learned that we could keep more of the fractional bits.

#### Multiplying

Multiplying does not require that the radix points be aligned before the operation is carried out. Let's assume we have two numbers as we had in our Adding example. A is an s11.4 precision number, which we move to A2, now an s27.4 large register (but still an s11.4 number of bits are in use). B is an s9.6 number, which we move to B2, now an s25.6 large register (but still an s9.6 number of bits are in use). C = A2 * B2 results in C being an s20.10 number. Note that C *is* using the entire 32-bit register for the result. Now, if we want the result squished back into a 16-bit register, then we must make some hard choices. Firstly, we already have 20 bits of integer precision—so any attempt (without looking at the number of bits actually used) to fit the result into a 16-bit register must result in some type of truncation. If we take the top 15 bits of the result (+1 bit for sign precision), we the programmers must remember that this is scaled up by 20-15 = 5 bits of precision. So even though we can fit the top 15 bits in a 16-bit register, we will lose the bottom 16 bits of precision, and we have to remember that the result is scaled by 5 bits (or integer 32). Interestingly, if we test both A and B beforehand, we may find that while they had the stated incoming precision by rote, they may not actually contain that number of live, set bits (e.g., if A is an s11.4 number by programmer's convention but its actual value is integer 33 or fixed-radix 2¹/₁₆, then we may not have as many bits to truncate in C).

## Conventions Used in the Library

This library uses radix points (binary points) instead of base-10 math, so for those who are not used to thinking in powers of two, I suggest glancing at the FR_main code, which contains a few examples of its use.

Mixed radix types for matrices and numbers are allowed and encouraged, but monitoring of mixed radix precision is left to the user of the functions.

Also, all types are typedef'd to represent the amount of bits available and the sign:
- `s8`  = signed 8-bit integer
- `s16` = signed 16-bit integer
- `s32` = signed 32-bit integer
- `u8`  = unsigned 8-bit integer
- `u16` = unsigned 16-bit integer
- `u32` = unsigned 32-bit integer

## This Library

This library includes a handful of functions for mixed integer radix math and for 2D coordinate transforms using fixed-radix math. This is especially useful on low-end embedded systems or systems without floating-point or SIMD support which need simple graphical transforms such as scale, rotate, skew, and translate of both 16- or 32-bit integer coordinates. The 2D matrix also allows for forward or backward transforms of points (e.g., camera-to-world and world-to-camera transformations).

Much of the library is given as MACROS (which always appear in UPPER CASE and always with the prefix `FR_`). The reason for this is it keeps compiled library size down, which can then be used as a shared object rather than depending on an incremental linker. However, if some of the macros are used a lot, it may be desirable to wrap them in a function call to trade inline expansion for code space reduction.

For example, the following wraps the macro `FR_ADD()` into a function to save code space:
```c
fr_add(s32 x, int xr, s32 y, int yr)
{
    return FR_ADD(x, xr, y, yr);
}
```

## Finally... A Few Thoughts on Higher-End CPUs, Embedded Processors, GPUs, and DSPs

DSPs (Digital Signal Processors) have built-in operations for saturating math. Usually, this is done by providing CPU instructions which have both traditional 2's complement and saturating math. For example, the low-level programmer can determine when two operands are multiplied *which* kind of multiply to use—saturated or unsaturated. This also applies to addition, subtraction, and combined multiply-accumulate instructions. 

Also, modern CPUs (80x86, x64, ARM, Power series, and DSPs) often include parallel instruction sets (SIMD). Examples include MMX, SSE, AltiVec, NEON, etc. These instruction sets allow block-loads of data into special registers which can execute multiple math operations in parallel. For example, in MMX or AltiVec, we can use 128-bit registers, each of which can hold four 32-bit integers. So now when we multiply 2 of these registers together, we get 4 simultaneous multiplies. Even better, the instructions are flexible in how they handle the internal data of the 128-bit integers and can treat a 128-bit register as 4 separate numbers, and the programmer can choose the saturating/non-saturating behavior. Most SIMD instruction sets allow the programmer to select the packing (four 32-bit numbers or eight 16-bit numbers or sixteen 8-bit numbers and even packed floating point) and signed/unsigned/saturated operations. 

Also similar is GPU or tensor computing hardware, which has usually been optimized for linear algebra/packed data operations. In these systems, packed data is also operated on en masse to provide high-performance vector computes.

More recent instruction sets combine advanced cryptographic, image processing, and transcendental functions for use in DSP-like media processing.

These powerful instruction sets can speed up repetitive matrix-oriented operations literally an order of magnitude or more compared with the regular CPU instruction stream, as long as the data is already packed in the correct format.

Thanks for taking a look!

Cheers, and I hope you find this useful.  
Feedback and bugs are welcome via GitHub issues.

## License

Licensed under BSD-2 License (see LICENSE.TXT included).
This library can be used freely in open source or commercial projects.  
(c) 2000-2025 Manjirnath (Manu) Chatterjee

## Version

Current version: 1.0.3

