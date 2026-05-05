# POSIX Example — Comprehensive FR_Math Demo

A full-featured desktop demo that exercises most of the FR_Math library
including fixed-point arithmetic, trig functions, error statistics,
2D matrix transforms, and formatted printing.

## What it demonstrates

| Topic | Functions / macros used |
|-------|----------------------|
| Overflow in 8-bit arithmetic | Raw C multiply showing wrap-around |
| Radix interpretation | `FR2D`, printing same integer at radixes 0-14 |
| Addition (with saturation) | `FR_FixAddSat` |
| Multiplication (with saturation) | `FR_FixMulSat` |
| 2D matrix transforms | `FR_Matrix2D_CPT`: translate, rotate, inverse, `XFormPtI`, `XFormPtI16` |
| Radix precision effects | Comparing radix 6 vs 11 for round-trip accuracy |
| Forward trig (optional) | `FR_CosI`, `FR_SinI`, `FR_TanI` sweep with error stats |
| Radian trig (optional) | `FR_cos` radian-native path |
| Inverse trig (optional) | `FR_acos` |
| Power / log (optional) | `FR_pow2`, `FR_EXP`, `FR_POW10` sweep |
| Formatted printing | `FR_printNumF` with `putchar` callback, `FR_CEIL`, `FR_FLOOR` |

Several test sections are gated by flags (`gTestForwardTrig`, etc.) near
the top of the file. Edit them to enable additional sweeps.

## Building

```bash
make            # compiles FR_Math_Example1
make run        # compiles and runs
make clean      # removes build artifacts
```

Or compile manually:

```bash
g++ -I../../src -Wall -Os \
    FR_Math_Example1.cpp ../../src/FR_math.c ../../src/FR_math_2D.cpp \
    -lm -o FR_Math_Example1
```

## Dependencies

- A C++ compiler (g++ or clang++)
- FR_Math source (`../../src/FR_math.c`, `../../src/FR_math.h`, `../../src/FR_defs.h`, `../../src/FR_math_2D.cpp`, `../../src/FR_math_2D.h`)
- Standard C math library (`-lm`)
