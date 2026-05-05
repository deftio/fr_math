# Fixed-Point Basics

An educational walkthrough of fixed-point fundamentals using the FR_Math
library. This example is self-contained and builds independently.

## What it demonstrates

| Section | Topic | Key functions / macros |
|---------|-------|----------------------|
| A | Same integer interpreted at radixes 0-15 | `FR2D` |
| B | Integer-to-fixed and back round-trip | `I2FR`, `FR2I` |
| C | Constructing fractional constants (pi) | `FR_NUM` |
| D | Add / subtract with radix alignment | `FR_ADD`, `FR_CHRDX` |
| E | Multiply: precision doubling and truncation | `FR_CHRDX` |
| F | Division (64-bit and 32-bit variants) | `FR_DIV`, `FR_DIV32` |
| G | Saturation vs overflow | `FR_FixAddSat`, `FR_FixMulSat` |
| H | Formatted output via putchar callback | `FR_printNumF` |

## Building

```bash
make            # compiles fixed_point_basics
make run        # compiles and runs
make clean      # removes build artifacts
```

Or compile manually:

```bash
g++ -I../../src -Wall -Os fixed_point_basics.cpp ../../src/FR_math.c -lm -o fixed_point_basics
```

## Expected output

The program prints a series of labeled sections. Section A shows how
the same raw integer (1234) maps to different floating-point values as
the radix changes. Subsequent sections print tables comparing FR_Math
operations against expected values, demonstrating precision, alignment
rules, and saturation behavior.

```
FR_Math — Fixed-Point Basics  (v...)

========================================
  A. Same integer at radixes 0-15
========================================

  Raw integer value: 1234
  ...

========================================
  H. FR_printNumF — formatted fixed-point printing
========================================
  ...

--- end ---
```

## Dependencies

- A C++ compiler (g++ or clang++)
- FR_Math source (`../../src/FR_math.c`, `../../src/FR_math.h`, `../../src/FR_defs.h`)
- Standard C math library (`-lm`, used only for `FR2D` debug macro)
