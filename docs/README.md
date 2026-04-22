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
16, and 24. Percent errors skip expected values near zero (|expected| < 0.01).

<!-- ACCURACY_TABLE_START -->
| Function | Max err (LSB) | Max err (%) | Avg err (%) | Note |
|---|---:|---:|---:|---|
| sin / cos | 7.5 | 0.7169 | 0.0100 | 65536-pt sweep + specials |
| tan | 38020.4 | 0.7118 | 0.0162 | 65536-pt sweep (skip poles) |
| asin / acos | 42.3 | 0.7025 | 0.0105 | 65536-pt; sqrt approx near boundary |
| atan2 | 63.3 | 0.4953 | 0.0268 | 65536x5 radii; asin/acos+hypot_fast8 |
| atan | 61.9 | 0.2985 | 0.0159 | 20001-pt sweep [-10,10]; via FR_atan2 |
| sqrt | 28.4 | 0.0003 | 0.0000 | Round-to-nearest |
| log2 | 10.5 | 0.2479 | 0.0045 | 65-entry mantissa table |
| pow2 | 220.4 | 0.1373 | 0.0057 | 65-entry fraction table |
| ln, log10 | 0.7 | 0.0015 | 0.0004 | Via FR_MULK28 from log2 |
| exp | 65.7 | 0.0719 | 0.0051 | FR_MULK28 + FR_pow2 |
| exp_fast | 195.5 | 0.0719 | 0.0064 | Shift-only scaling |
| pow10 | 143.4 | 0.1163 | 0.0075 | FR_MULK28 + FR_pow2 |
| pow10_fast | 581.9 | 0.1163 | 0.0100 | Shift-only scaling |
| hypot (exact) | 0.2 | 0.0001 | 0.0000 | 64-bit intermediate |
| hypot_fast8 (8-seg) | 59968.8 | 0.0977 | 0.0508 | Shift-only, no multiply |
<!-- ACCURACY_TABLE_END -->

## What's in the box

| Area | Functions |
| --- | --- |
| Arithmetic | `FR_ADD`, `FR_SUB`, `FR_DIV`, `FR_DIV32`, `FR_MOD`, `FR_FixMuls`, `FR_FixMulSat`, `FR_CHRDX` |
| Utility | `FR_MIN`, `FR_MAX`, `FR_CLAMP`, `FR_ABS`, `FR_SGN` |
| Trig (integer deg) | `FR_Sin`, `FR_Cos`, `FR_Tan`, `FR_SinI`, `FR_CosI`, `FR_TanI` |
| Trig (radian/BAM) | `fr_sin`, `fr_cos`, `fr_tan`, `fr_sin_bam`, `fr_cos_bam`, `fr_sin_deg`, `fr_cos_deg` |
| Inverse trig | `FR_atan`, `FR_atan2`, `FR_asin`, `FR_acos` |
| Log / exp | `FR_log2`, `FR_ln`, `FR_log10`, `FR_pow2`, `FR_EXP`, `FR_POW10`, `FR_EXP_FAST`, `FR_POW10_FAST`, `FR_MULK28` |
| Roots | `FR_sqrt`, `FR_hypot`, `FR_hypot_fast8` |
| Wave generators | `fr_wave_sqr`, `fr_wave_pwm`, `fr_wave_tri`, `fr_wave_saw`, `fr_wave_tri_morph`, `fr_wave_noise` |
| Envelope | `fr_adsr_init`, `fr_adsr_trigger`, `fr_adsr_release`, `fr_adsr_step` |
| 2D transforms | `FR_Matrix2D_CPT` (mul, add, sub, det, inv, setrotate, XFormPtI, XFormPtI16) |
| Formatted output | `FR_printNumD`, `FR_printNumF`, `FR_printNumH`, `FR_numstr` |

Every function is covered by the TDD characterization suite in
[`tests/test_tdd.cpp`](../tests/test_tdd.cpp).

## Lean build options

Two compile-time `#define` guards let you strip optional subsystems
for ROM-constrained targets. Define them before including `FR_math.h`
(or pass `-D` on the compiler command line):

| Define | What it removes | Typical savings |
|---|---|---|
| `FR_NO_PRINT` | `FR_printNumF`, `FR_printNumD`, `FR_printNumH`, `FR_numstr` | ~1.3 KB |
| `FR_NO_WAVES` | `fr_wave_*` (6 shapes), `fr_adsr_*` (ADSR envelope), `FR_HZ2BAM_INC` | ~0.6 KB |

With both guards enabled the core math library (trig, inverse trig, log/exp,
sqrt, hypot) compiles to ~3.5 KB on x86-64 / clang -Os. On Thumb-2 this
would be roughly 2.6 KB.

```c
/* Example: headless sensor node — math only, no print, no audio */
#define FR_NO_PRINT
#define FR_NO_WAVES
#include "FR_math.h"
```

With `-ffunction-sections` and linker `--gc-sections`, the linker will
also strip any unused functions automatically, so these guards are most
useful when you include the library as a single `.c` file or static
archive without section-level dead-code elimination.

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

/* ---- Creating fixed-point values ----
 *
 * FR_NUM(integer, frac_digits, num_digits, radix) encodes a decimal
 * literal at compile time.  The fractional part is the digits AFTER
 * the decimal point, and num_digits says how many digits that is.
 * Think: FR_NUM(3, 14159, 5, 16) means "3.14159" at radix 16.
 */
s32 pi   = FR_NUM(3, 14159, 5, R);  /* 3.14159 → raw 205886 at r16  */
s32 half = FR_NUM(0, 5, 1, R);      /* 0.5     → raw 32768           */
s32 neg  = FR_NUM(-2, 75, 2, R);    /* -2.75   → raw -180224         */

/* Or parse from a string at runtime (no floats, no strtod): */
s32 pi2  = FR_numstr("3.14159", R); /* same result as FR_NUM above    */

/* Integer-to-fixed: I2FR(n, radix) just shifts left */
s32 two  = I2FR(2, R);              /* 2.0 → raw 131072              */

/* ---- Naming convention: macros vs functions ----
 *
 * UPPERCASE FR_ names are macros — they expand inline with no call
 * overhead, and the compiler can constant-fold them.  Use these for
 * conversions and simple arithmetic:
 *   I2FR, FR2I, FR_NUM, FR_ADD, FR_DIV, FR_ABS, FR_CHRDX, FR_EXP ...
 *
 * MixedCase FR_ names are functions — they contain loops, tables, or
 * multi-step algorithms where inlining would waste ROM:
 *   FR_Cos, FR_sqrt, FR_atan2, FR_log2, FR_pow2, FR_printNumF ...
 *
 * lowercase fr_ names are v2 functions (radian trig, wave generators,
 * ADSR envelopes):
 *   fr_sin, fr_cos, fr_tan, fr_wave_tri, fr_adsr_step ...
 *
 * Some macros wrap functions: FR_EXP(x,r) scales x then calls
 * FR_pow2 — one-liner convenience, heavy lifting in the function.
 */

/* ---- Math functions ---- */
s32 c45   = FR_Cos(45, 0);                /* cos(45°) = 0.7071       */
s32 s30   = fr_sin(FR_numstr("0.5236", R), R); /* sin(0.5236 rad)    */
s32 root2 = FR_sqrt(two, R);              /* sqrt(2)  = 1.4142       */
s32 angle = FR_atan2(I2FR(1,R), I2FR(1,R), R); /* atan2(1,1) rad     */
s32 lg    = FR_log2(I2FR(1000, R), R, R); /* log2(1000) ~ 9.97       */
s32 ex    = FR_EXP(I2FR(1, R), R);        /* macro: scales then calls
                                            * FR_pow2 internally      */

/* ---- Printing (serial / UART / file friendly) ----
 *
 * FR_printNumF takes a per-character output function — works with
 * putchar, Serial.write, UART_putc, or any int(*)(char).  No
 * sprintf, no floats, no heap.  Ideal for bare-metal targets.
 */
int my_putchar(char c) { return putchar(c); }  /* or your UART func */

FR_printNumF(my_putchar, pi, R, 8, 5);    /* prints " 3.14159"      */
FR_printNumF(my_putchar, neg, R, 8, 2);   /* prints "   -2.75"      */
FR_printNumD(my_putchar, FR2I(lg, R), 4); /* prints "   9" (integer)*/
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
| Multiply-free option | No | No | Yes (e.g. `FR_EXP_FAST`, `FR_hypot_fast8`) |
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
RISC-V, and various 8/16-bit embedded targets. v2.0.6 is the current
release with a full test suite, bit-exact numerical
specification, and CI on every push.

## License

BSD-2-Clause. Use it freely in open source or commercial projects.
