# FR_Math Documentation

**A C language fixed-point math library for embedded systems.**

FR_Math is a compact, integer-only fixed-point math library built for
systems where floating point is too slow, too big, or unavailable. Designed for embedded targets ranging from
legacy 16 MHz 68k processors to modern Cortex-M and RISC-V cores, it
provides a full suite of math primitives — trigonometry, logarithms,
roots, transforms, and signal generators — while remaining
deterministic, portable, and small. Unlike traditional fixed-point
libraries, FR_Math lets the caller choose the binary point per
operation, trading precision and range explicitly instead of locking
into a single format.

- Pure C (C99/C11/C17) with an optional C++ 2D-transform wrapper.
  Tested on gcc, clang, MSVC, IAR, Keil, sdcc, AVR-gcc, MSP430-gcc,
  RISC-V toolchains, and Arduino.
- Zero dependencies beyond `<stdint.h>`.
- Parameterised radix: every function takes the binary point as an
  argument, so you choose how many fractional bits you need per call.
- Deterministic, bounded error — every public symbol has a documented
  worst case in the [API reference](api-reference.md).

## Contents

This directory holds plain-text markdown documentation so you can
read everything directly in a terminal or editor, without a browser
or any tooling. If you want the browser version, look in
[`../pages/`](../pages/) or open the deployed GitHub Pages site at
<https://deftio.github.io/fr_math/>.

| Page | What it covers |
| --- | --- |
| [getting-started.md](getting-started.md) | Clone, build, run your first FR_Math program. |
| [fixed-point-primer.md](fixed-point-primer.md) | Why fixed-point exists, sM.N notation, operations, how to pick a radix. |
| [api-reference.md](api-reference.md) | Every public symbol: signature, radix, precision, error behaviour. |
| [examples.md](examples.md) | Runnable snippets: trig, log, waves, ADSR, 2D transforms. |
| [building.md](building.md) | Makefile, scripts, test suite, coverage, cross-compilation. |
| [releases.md](releases.md) | Release history with per-version highlights and breaking changes. |

## Measured accuracy

Errors below are measured at Q16.16 (s15.16). All functions accept any
radix — Q16.16 is just the reference point for the table. See the
[TDD report](../build/test_tdd_report.md) for sweeps at radixes 8, 12,
16, and 24.

| Function | Max error | Note |
|---|---|---|
| sin / cos | 5 LSB (~7.7e-5) | Exact at 0, 90, 180, 270 |
| sqrt | ≤ 0.5 LSB | Round-to-nearest |
| log2 | ≤ 4 LSB | 65-entry mantissa table |
| pow2 | ≤ 1 LSB (integers exact) | 65-entry fraction table |
| ln, log10 | ≤ 4 LSB | Via FR_MULK28 from log2 |
| hypot (exact) | ≤ 0.5 LSB | 64-bit intermediate |
| hypot_fast (4-seg) | 0.34% | Shift-only, no multiply |
| hypot_fast8 (8-seg) | 0.10% | Shift-only, no multiply |

## What's in the box

| Area | Functions |
| --- | --- |
| Arithmetic | `FR_ADD`, `FR_SUB`, `FR_DIV`, `FR_DIV32`, `FR_MOD`, `FR_FixMuls`, `FR_FixMulSat`, `FR_CHRDX` |
| Utility | `FR_MIN`, `FR_MAX`, `FR_CLAMP`, `FR_ABS`, `FR_SGN` |
| Trig (integer deg) | `FR_Sin`, `FR_Cos`, `FR_Tan`, `FR_SinI`, `FR_CosI`, `FR_TanI` |
| Trig (radian/BAM) | `fr_sin`, `fr_cos`, `fr_tan`, `fr_sin_bam`, `fr_cos_bam`, `fr_sin_deg`, `fr_cos_deg` |
| Inverse trig | `FR_atan`, `FR_atan2`, `FR_asin`, `FR_acos` |
| Log / exp | `FR_log2`, `FR_ln`, `FR_log10`, `FR_pow2`, `FR_EXP`, `FR_POW10`, `FR_EXP_FAST`, `FR_POW10_FAST`, `FR_MULK28` |
| Roots | `FR_sqrt`, `FR_hypot`, `FR_hypot_fast`, `FR_hypot_fast8` |
| Wave generators | `fr_wave_sqr`, `fr_wave_pwm`, `fr_wave_tri`, `fr_wave_saw`, `fr_wave_tri_morph`, `fr_wave_noise` |
| Envelope | `fr_adsr_init`, `fr_adsr_trigger`, `fr_adsr_release`, `fr_adsr_step` |
| 2D transforms | `FR_Matrix2D_CPT` (mul, add, sub, det, inv, setrotate, XFormPtI, XFormPtI16) |
| Formatted output | `FR_printNumD`, `FR_printNumF`, `FR_printNumH`, `FR_numstr` |

Every function is covered by the TDD characterization suite in
[`tests/test_tdd.cpp`](../tests/test_tdd.cpp).

## Why fixed-point, in 2026?

Most application code today has an FPU and can use `float` freely.
But there are still large, interesting corners where fixed-point
pays off:

- **8- and 16-bit MCUs** (AVR, MSP430, 8051, sdcc) where the FPU does
  not exist and even software float is too slow or too large.
- **Hot inner loops on any CPU** where a parameterised-radix integer
  multiply is faster and more deterministic than a `float`. Think DSP
  taps, PID loops, coordinate transforms inside a scanline renderer.
- **Bit-exact reproducibility** across compilers, architectures, and
  hosts — something IEEE float does *not* give you in the general
  case.
- **ROM-tight builds** where linking `libm` or `libgcc_s` pulls in
  more code than the whole application logic.

FR_Math is engineered for these use cases. It does not try to be a
generic `float` replacement.

## Quick taste

```c
#include "FR_math.h"

#define R 16  /* work at radix 16 (s15.16) throughout */

s32 pi    = FR_NUM(3, 14159, 5, R);       /* pi at radix 16             */
s32 c45   = FR_CosI(45);                  /* cos 45 deg = 0.7071 (s15.16) */
s32 root2 = FR_sqrt(I2FR(2, R), R);       /* sqrt(2)    = 1.4142        */
s32 lg    = FR_log2(I2FR(1000, R), R, R); /* log2(1000) ~ 9.97          */
s32 ex    = FR_EXP(I2FR(1, R), R);        /* e^1        ~ 2.7183        */
```

See [getting-started.md](getting-started.md) for a complete
walkthrough, or jump straight to
[fixed-point-primer.md](fixed-point-primer.md) if you want to
understand *how* the radix notation works first.

## Comparison

| Feature | libfixmath | CMSIS-DSP | FR_Math |
|---|---|---|---|
| Fixed format | Q16.16 only | Q31 / Q15 | Any radix |
| Angle input | Radians (Q16.16) | Radians (float) | BAM (u16), degrees, or radians |
| Exact cardinal angles | No | N/A | Yes |
| Multiply-free option | No | No | Yes (e.g. `FR_EXP_FAST`, `FR_hypot_fast`) |
| Wave generators | No | No | 6 shapes + ADSR |
| Dependencies | None | ARM only | None |
| Code size (Cortex-M0, -Os) | 2.4 KB | ~40 KB+ | 4.2 KB |

Sizes measured with `arm-none-eabi-gcc -mcpu=cortex-m0 -mthumb -Os`.
libfixmath covers trig/sqrt/exp in Q16.16 only; FR_Math includes
log/ln/log10, wave generators, ADSR, print helpers, and variable radix.
CMSIS-DSP estimate is for the math function subset only.
See [`docker/build_sizes.sh`](../docker/build_sizes.sh) for the build
script.

## History

FR_Math has been in service since **2000**, originally built for
graphics transforms on 16 MHz 68k Palm Pilots (it shipped inside
Trumpetsoft's *Inkstorm*), then ported forward to ARM, x86, MIPS,
RISC-V, and various 8/16-bit embedded targets. v2.0.1 is the current
release with a full test suite, bit-exact numerical
specification, and CI on every push.

## License

BSD-2-Clause. Use it freely in open source or commercial projects.
