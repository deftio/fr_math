# FR_Math Documentation

**A C language fixed-point math library for embedded systems.**

FR_Math provides trig, log/exp, 2D transforms, square root, and a
small DSP toolkit (wave generators, ADSR envelope) — all running on
plain integer registers. No floating-point unit required, no
floating-point library required, no runtime allocation. The library has
shipped on processors that predate MMX and is still maintained today.

- Works on any C99 toolchain — gcc, clang, MSVC, IAR, Keil, sdcc,
  AVR-gcc, MSP430-gcc, RISC-V.
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

## What's in the box

| Area | Functions |
| --- | --- |
| Arithmetic | `FR_ADD`, `FR_SUB`, `FR_DIV`, `FR_DIV32`, `FR_MOD`, `FR_FixMuls`, `FR_FixMulSat`, `FR_CHRDX` |
| Utility | `FR_MIN`, `FR_MAX`, `FR_CLAMP`, `FR_ABS`, `FR_SGN` |
| Trig (integer deg) | `FR_Sin`, `FR_Cos`, `FR_Tan`, `FR_SinI`, `FR_CosI`, `FR_TanI` |
| Trig (radian/BAM) | `fr_sin`, `fr_cos`, `fr_tan`, `fr_sin_bam`, `fr_cos_bam`, `fr_sin_deg`, `fr_cos_deg` |
| Inverse trig | `FR_atan`, `FR_atan2`, `FR_asin`, `FR_acos` |
| Log / exp | `FR_log2`, `FR_ln`, `FR_log10`, `FR_pow2`, `FR_EXP`, `FR_POW10` |
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
#include "FR_defs.h"
#include "FR_math.h"

/* Compute 3.5 * 2.25 at radix 4 (s?.4 format). */
s32 a = I2FR(3, 4) | (1 << 3);   /* 3.5  = 0x38 */
s32 b = I2FR(2, 4) | (1 << 2);   /* 2.25 = 0x24 */
s32 c = (s32)(((int64_t)a * b) >> 4); /* c ≈ 7.875, also at radix 4 */

/* Rotate a point 30° around the origin. */
#include "FR_math_2D.h"
FR_Matrix2D_CPT R;
R.ID();
R.setrotate(30);                 /* 30 degrees */
s32 px = I2FR(100, 8), py = I2FR(0, 8);
s32 rx, ry; R.XFormPtI(px, py, &rx, &ry); /* result ~(86.6, 50.0) at radix 8 */
```

See [getting-started.md](getting-started.md) for a complete
walkthrough, or jump straight to
[fixed-point-primer.md](fixed-point-primer.md) if you want to
understand *how* the radix notation works first.

## History

FR_Math has been in service since **2000**, originally built for
graphics transforms on 16 MHz 68k Palm Pilots (it shipped inside
Trumpetsoft's *Inkstorm*), then ported forward to ARM, x86, MIPS,
RISC-V, and various 8/16-bit embedded targets. v2.0.1 is the current
release with a full test suite, bit-exact numerical
specification, and CI on every push.

## License

BSD-2-Clause. Use it freely in open source or commercial projects.
