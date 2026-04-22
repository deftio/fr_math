[![License](https://img.shields.io/badge/License-BSD%202--Clause-blue.svg)](https://opensource.org/licenses/BSD-2-Clause)
[![CI](https://github.com/deftio/fr_math/actions/workflows/ci.yml/badge.svg)](https://github.com/deftio/fr_math/actions/workflows/ci.yml)
[![Coverage](https://img.shields.io/badge/coverage-99%25-brightgreen.svg)](#building-and-testing)
[![Docs](https://img.shields.io/badge/docs-online-blue.svg)](https://deftio.github.io/fr_math/)
[![Version](https://img.shields.io/badge/version-2.0.6-blue.svg)](release_notes.md)

# FR_Math: A C Language Fixed-Point Math Library for Embedded Systems

FR_Math is a compact, integer-only fixed-point math library built for
systems where floating point is too slow, too big, or unavailable. Designed for embedded targets ranging from
legacy 16 MHz 68k processors to modern Cortex-M and RISC-V cores, it
provides a full suite of math primitives — trigonometry, logarithms,
roots, transforms, and signal generators — while remaining
deterministic, portable, and small. Unlike traditional fixed-point
libraries, FR_Math lets the caller choose the binary point per
operation, trading precision and range explicitly instead of locking
into a single format. Pure C (C99/C11/C17) with an optional C++
2D-transform wrapper. Compiles under Arduino. Zero dependencies
beyond `<stdint.h>`.

### Library size (FR_math.c only, `-Os`)

The following are compilied object code sizes on select platforms from static test build.  Your sizes may vary depending on optimization choices and linker settings.  Sizes include full code and any internal tables and are ROMable.

| Target | Code (text) |
|--------|-------------|
| Cortex-M0 (Thumb-1) | 4.2 KB |
| Cortex-M4 (Thumb-2) | 4.1 KB |
| ESP32 (Xtensa) | 4.6 KB |
| 68k | 5.5 KB |
| x86-64 | 5.8 KB |
| RISC-V 32 (rv32im) | 6.5 KB |
| x86-32 | 7.2 KB |
| MSP430 (16-bit) | 8.4 KB |
| 8051 (SDCC) | 20.4 KB * |

The optional 2D module adds ~1 KB.
\* 8051 and MSP430 are 8/16-bit — every 32-bit operation expands to multiple instructions.
See [`docker/`](docker/) for the cross-compile setup.

### Measured accuracy

Errors below are measured at Q16.16 (s15.16). All functions accept any
radix — Q16.16 is just the reference point for the table.
Percent errors skip expected values near zero (|expected| < 0.01).

Note that at other radixes (3bit, 24 bit etc), accuracy may change due fractional bits available but with increased/decreased scale.  All functions support 0 to 30 bit radix types at compile time.

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

### What's in the box

| Area | Functions |
|---|---|
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

## Quick start

```bash
git clone https://github.com/deftio/fr_math.git
cd fr_math
make lib       # build static library
make test      # run all tests (coverage, TDD characterization, 2D)
```

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
 *   I2FR, FR2I, FR_NUM, FR_ADD, FR_MUL, FR_DIV, FR_ABS, FR_EXP ...
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

## Documentation

The full docs ship in two forms — pick whichever fits how you read.

**Browser (rendered HTML):**

- **Online:** <https://deftio.github.io/fr_math/>
- **Local:** [pages/index.html](pages/index.html) — static HTML/CSS/JS, no build step.

**Terminal / editor (plain markdown):**

- [docs/README.md](docs/README.md) — same content as plain markdown.
  - [getting-started.md](docs/getting-started.md) | [fixed-point-primer.md](docs/fixed-point-primer.md) | [api-reference.md](docs/api-reference.md)
  - [examples.md](docs/examples.md) | [building.md](docs/building.md) | [releases.md](docs/releases.md)

## History

FR_Math has been in service since 2000, originally built for graphics
transforms on 16 MHz 68k Palm Pilots. It shipped inside Trumpetsoft's
*Inkstorm* on PalmOS, then moved forward through ARM, x86, MIPS,
RISC-V, and various 8/16-bit embedded targets. v2.0.2 is the current
release with a full test suite, bit-exact numerical specification, and
CI on every push.

## License

BSD-2-Clause — see [LICENSE.txt](LICENSE.txt).
(c) 2000-2026 M. Chatterjee

## For AI coding agents

- [llms.txt](llms.txt) — machine-readable API summary
- [agents.md](agents.md) — conventions, build commands, and contribution guide for coding agents

## Version

2.0.2 — see [release_notes.md](release_notes.md) for the v1 → v2
migration guide, numerical fixes, and new functionality.
