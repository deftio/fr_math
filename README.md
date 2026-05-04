[![License](https://img.shields.io/badge/License-BSD%202--Clause-blue.svg)](https://opensource.org/licenses/BSD-2-Clause)
[![CI](https://github.com/deftio/fr_math/actions/workflows/ci.yml/badge.svg)](https://github.com/deftio/fr_math/actions/workflows/ci.yml)
[![Coverage](https://img.shields.io/badge/coverage-98%25-brightgreen.svg)](#building-and-testing)
[![Docs](https://img.shields.io/badge/docs-online-blue.svg)](https://deftio.github.io/fr_math/)
[![Version](https://img.shields.io/badge/version-2.0.8-blue.svg)](release_notes.md)

[![PlatformIO](https://img.shields.io/badge/PlatformIO-library-teal.svg)](https://registry.platformio.org/libraries/deftio/fr_math)
[![Arduino](https://img.shields.io/badge/Arduino-library-teal.svg)](https://github.com/deftio/fr_math)
[![ESP Component](https://img.shields.io/badge/ESP--IDF-component-teal.svg)](https://components.espressif.com/components/deftio/fr_math)

# FR_Math: A C Language Fixed-Point Math Library for Embedded Systems

See: **[Documentation & Guide](https://deftio.github.io/fr_math/)** — for API reference, examples, fixed-point primer, build instructions.  

 
**FR_Math** is a compact, integer-only fixed-point math library built for systems where floating point is too slow, too big, or unavailable. Designed for embedded targets ranging from legacy 16 MHz 68k processors to modern Cortex-M and RISC-V cores, it provides a full suite of math primitives — trigonometry, logarithms, roots, transforms, and signal generators — while remaining deterministic, portable, and small.  Optional print utility functions are also provided for pretty printing out fixed point numbers over serial links or buffers.
 
Unlike most fixed-point libraries, FR_Math lets the caller choose the binary point (raddix) per operation, trading precision and range explicitly instead of locking into a single format. FR_math is Pure C (C99/C11/C17,with) with C++ wrappers.
Compiles under Arduino, PlatformIO, Espressif, many older embedded targets.   
Zero dependencies beyond `<stdint.h>`.


### Measured accuracy

Errors below are measured at Q16.16 (s15.16). All functions accept any
radix — Q16.16 is just the reference point for the table.

At other radixes (3-bit, 24-bit, etc.) accuracy will differ due to the
number of fractional bits available.

| Function | Max err (%)* | Avg err (%) | Note |
| --- | --- | --- | --- |
| sin/cos (BAM) | 0.1526 | 0.0030 | fr_sin_bam/fr_cos_bam direct; 129-entry table |
| sin/cos (deg) | 0.1526 | 0.0029 | FR_Sin/FR_Cos ±360° s15.16; FR_DEG2BAM |
| sin/cos (rad) | 0.1828 | 0.0033 | fr_sin/fr_cos via fr_rad_to_bam ±2π r16 |
| tan (BAM) | 0.5823 | 0.0008 | fr_tan_bam 65536-pt full; ±maxint at poles |
| tan (deg) | 0.5311 | 0.0008 | fr_tan_deg ±360° s15.16 full; sat at poles |
| tan (rad) | 0.0386 | 0.0001 | fr_tan ±2π r16; r24 pole bypass |
| asin / acos | 0.7771 | 0.0280 | 65536-pt; sqrt approx near boundary |
| atan2 | 0.2564 | 0.0237 | 65536x5 radii; asin/acos+hypot_fast8 |
| atan | 0.2425 | 0.0155 | 20001-pt full sweep [-10,10]; via FR_atan2 |
| sqrt | 0.0000 | 0.0000 | Round-to-nearest |
| log2 | 0.0116 | 0.0016 | 65-entry mantissa table |
| pow2 | 0.0018 | 0.0004 | 65-entry fraction table |
| ln, log10 | 0.0004 | 0.0000 | Via FR_MULK28 from log2 |
| exp | 0.0003 | 0.0000 | FR_MULK28 + FR_pow2 |
| exp_fast | 0.0009 | 0.0001 | Shift-only scaling |
| pow10 | 0.0005 | 0.0000 | FR_MULK28 + FR_pow2 |
| pow10_fast | 0.0022 | 0.0002 | Shift-only scaling |
| hypot (exact) | 0.0000 | 0.0000 | 64-bit intermediate |
| hypot_fast8 (8-seg) | 0.0915 | 0.0320 | Shift-only, no multiply |

 
### What's in the box

| Area | Functions |
| --- | --- |
| Arithmetic | FR_ADD, FR_SUB, FR_DIV, FR_DIV32, FR_MOD, FR_FixMuls, FR_FixMulSat, FR_CHRDX |
| Utility | FR_MIN, FR_MAX, FR_CLAMP, FR_ABS, FR_SGN |
| Trig (radian/BAM) | fr_sin, fr_cos, fr_tan, fr_sin_bam, fr_cos_bam, fr_tan_bam |
| Trig (degree) | fr_sin_deg, fr_cos_deg, fr_tan_deg, FR_SinI, FR_CosI, FR_TanI |
| Inverse trig | FR_atan, FR_atan2, FR_asin, FR_acos |
| Log / exp | FR_log2, FR_ln, FR_log10, FR_pow2, FR_EXP, FR_POW10, FR_EXP_FAST, FR_POW10_FAST, FR_MULK28 |
| Roots | FR_sqrt, FR_hypot, FR_hypot_fast8 |
| Wave generators | fr_wave_sqr, fr_wave_pwm, fr_wave_tri, fr_wave_saw, fr_wave_tri_morph, fr_wave_noise |
| Envelope | fr_adsr_init, fr_adsr_trigger, fr_adsr_release, fr_adsr_step |
| 2D transforms | FR_Matrix2D_CPT (mul, add, sub, det, inv, setrotate, XFormPtI, XFormPtI16) |
| Formatted output | FR_printNumD, FR_printNumF, FR_printNumH, FR_numstr |

### Library size (FR_math.c only, `-Os`)

Compiled object code sizes on select platforms (static test build). Your
sizes may vary depending on optimization and linker settings. Sizes
include all code and internal tables; everything is ROMable.

<!-- SIZE_TABLE_START -->
| Target | Lean | Core | Full |
| --- | ---:| ---:| ---:|
| Cortex-M4 (STM32) | 3.3 KB | 4.4 KB | 5.5 KB |
| Cortex-M0 (RP2040) | 3.4 KB | 4.5 KB | 5.7 KB |
| RISC-V rv64 | 4.0 KB | 5.5 KB | 6.8 KB |
| RISC-V rv32 | 4.1 KB | 5.5 KB | 6.8 KB |
| Xtensa LX106 (ESP8266) | 4.2 KB | 5.8 KB | 7.3 KB |
| 68k | 4.4 KB | 6.2 KB | 7.8 KB |
| x86-64 (GCC) | 4.6 KB | 6.1 KB | 8.0 KB |
| AArch64 (ARM64) | 4.8 KB | 6.6 KB | 8.7 KB |
| x86-32 | 5.3 KB | 7.2 KB | 9.2 KB |
| MSP430 (16-bit) | 7.8 KB | 10.7 KB | 12.8 KB |
| AVR (ATmega328P) | 9.2 KB | 12.8 KB | 15.4 KB |
| 68HC11 | 13.3 KB | 18.4 KB | 22.6 KB |
<!-- SIZE_TABLE_END -->

Lean = `-DFR_LEAN -DFR_NO_PRINT` (radian trig, inv trig, log/exp, sqrt).
Core = `-DFR_CORE_ONLY` (+ degree trig, BAM tan, log10, hypot).
Full = all features (+ print, waves, ADSR).
The optional 2D module adds ~1 KB.
\* MSP430, 68HC11, and AVR are 8/16-bit — every 32-bit operation expands to multiple instructions.
See [Building & Testing](docs/building.md) for the full cross-compile setup.

### Lean build options

Three compile-time `#define` guards let you strip optional subsystems
for ROM-constrained targets. Define them before including `FR_math.h`
(or pass `-D` on the compiler command line):

| Define | What it removes | Typical savings |
| --- | --- | --- |
| FR_CORE_ONLY | Everything below (print + waves) | ~1.9 KB |
| FR_NO_PRINT | FR_printNumF, FR_printNumD, FR_printNumH, FR_numstr | ~1.3 KB |
| FR_NO_WAVES | fr_wave_* (6 shapes), fr_adsr_* (ADSR envelope), FR_HZ2BAM_INC | ~0.6 KB |

`FR_CORE_ONLY` is a convenience shorthand that defines both
`FR_NO_PRINT` and `FR_NO_WAVES` in one step.

```c
/* Example: headless sensor node — math only, no print, no audio */
#define FR_CORE_ONLY
#include "FR_math.h"
```

With `-ffunction-sections` and linker `--gc-sections`, the linker will
also strip any unused functions automatically, so these guards are most
useful when you include the library as a single `.c` file or static
archive without section-level dead-code elimination.

## Quick start

```bash
git clone https://github.com/deftio/fr_math.git
cd fr_math
make lib       # build static library
make test      # run all tests (unit, TDD characterization, 2D)
```

## Example

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
 *   FR_sqrt, FR_atan2, FR_log2, FR_pow2, FR_printNumF ...
 *
 * lowercase fr_ names are v2 functions (degree/radian/BAM trig, wave
 * generators, ADSR envelopes):
 *   fr_sin_deg, fr_cos_deg, fr_tan_deg, fr_sin, fr_cos, fr_tan,
 *   fr_wave_tri, fr_adsr_step ...
 *
 * Legacy aliases: FR_Cos, FR_Sin, FR_Tan still work — they are
 * macros that map to fr_cos_deg, fr_sin_deg, fr_tan_deg.  New code
 * should use the fr_ names directly.
 *
 * Some macros wrap functions: FR_EXP(x,r) scales x then calls
 * FR_pow2 — one-liner convenience, heavy lifting in the function.
 */

/* ---- Math functions ---- */
s32 c45   = fr_cos_deg(45, 0);            /* cos(45°) = 0.7071       */
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
RISC-V, and various 8/16-bit embedded targets.   
The current release now has a full test suite, bit-exact numerical specification, and
CI on every push and better documentation.
 
## License

BSD-2-Clause — see [LICENSE.txt](LICENSE.txt).
(c) 2000-2026 M. Chatterjee

PRs and suggestions are welcome.  Please be detailed as embedded systems can involve many tradeoffs.

## For AI coding agents

- [llms.txt](llms.txt) — machine-readable API summary
- [agents.md](agents.md) — conventions, build commands, and contribution guide for coding agents

## Version

2.0.7 — see [release_notes.md](release_notes.md) for the v1 → v2
migration guide, numerical fixes, and new functionality.