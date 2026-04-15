[![License](https://img.shields.io/badge/License-BSD%202--Clause-blue.svg)](https://opensource.org/licenses/BSD-2-Clause)
[![CI](https://github.com/deftio/fr_math/actions/workflows/ci.yml/badge.svg)](https://github.com/deftio/fr_math/actions/workflows/ci.yml)
[![Coverage](https://img.shields.io/badge/coverage-99%25-brightgreen.svg)](#building-and-testing)
[![Tests](https://img.shields.io/badge/tests-42%20passing-brightgreen.svg)](#building-and-testing)
[![Docs](https://img.shields.io/badge/docs-online-blue.svg)](https://deftio.github.io/fr_math/)
[![Version](https://img.shields.io/badge/version-2.0.1-blue.svg)](release_notes.md)
![No FPU](https://img.shields.io/badge/FPU-not%20required-orange.svg)
![Code size](https://img.shields.io/badge/Cortex--M4-3.6%20KB-informational.svg)

# FR_Math: A C Language Fixed-Point Math Library for Embedded Systems

FR_Math provides trig, log/exp, square root, 2D transforms, wave
generators, and an ADSR envelope — all in integer-only C99. It replaces
`libm` on targets that lack an FPU or where code size and determinism
matter. The library compiles with gcc, clang, MSVC, IAR, Keil, SDCC,
and AVR-gcc. Zero dependencies beyond `<stdint.h>`.

### Library size (FR_math.c only, `-Os`)

| Target | Code (text) |
|--------|-------------|
| Cortex-M0 (Thumb-1) | 3.7 KB |
| Cortex-M4 (Thumb-2) | 3.6 KB |
| ESP32 (Xtensa) | 4.1 KB |
| 68k | 5.0 KB |
| x86-64 | 5.3 KB |
| RISC-V 32 (rv32im) | 5.9 KB |
| x86-32 | 6.6 KB |
| MSP430 (16-bit) | 7.9 KB |
| 8051 (SDCC) | 18.1 KB * |

Sizes are code-only (text section). The optional 2D module adds ~1 KB.
\* 8051 and MSP430 are 8/16-bit — every 32-bit operation expands to multiple instructions.
See [`docker/`](docker/) for the cross-compile setup.

### Measured accuracy (Q16.16 unless noted)

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

### What's in the box

| Area | Functions |
|---|---|
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

s32 pi    = FR_NUM(3, 14159, 5, R);       /* pi at radix 16             */
s32 c45   = FR_CosI(45);                  /* cos 45 deg = 0.7071 (s15.16) */
s32 root2 = FR_sqrt(I2FR(2, R), R);       /* sqrt(2)    = 1.4142        */
s32 lg    = FR_log2(I2FR(1000, R), R, R); /* log2(1000) ~ 9.97          */
s32 ex    = FR_EXP(I2FR(1, R), R);        /* e^1        ~ 2.7183        */
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
RISC-V, and various 8/16-bit embedded targets. v2.0.1 is the current
release with a full test suite, bit-exact numerical specification, and
CI on every push.

## License

BSD-2-Clause — see [LICENSE.txt](LICENSE.txt).
(c) 2000-2026 M. Chatterjee

## Version

2.0.1 — see [release_notes.md](release_notes.md) for the v1 → v2
migration guide, numerical fixes, and new functionality.
