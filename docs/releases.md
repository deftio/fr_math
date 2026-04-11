# Releases

Release highlights. For the full per-symbol change log, see
[release_notes.md](https://github.com/deftio/fr_math/blob/master/release_notes.md)
in the repo.

## v2.0.0 — 2026

The first major revision in more than twenty years. v2 is a
bug-fix, precision, and feature release with a full test suite and
99% line coverage.

### 64-bit safety

v1 defined `s32` as `signed long`, which is
64 bits on LP64 platforms (Linux x64, macOS arm64). Every
fixed-point path that assumed `s32` was exactly 32 bits
silently produced wrong results on desktop hosts. v2 migrates all
typedefs to `<stdint.h>`
(`int8_t`…`int32_t`). C99 is now
mandatory.

### Numerical fixes

- `FR_FixMulSat` / `FR_FixMuls`: rewritten
  on an `int64_t` fast path with explicit saturation;
  v1 had an algebraic bug in the sign-combination logic.
- `FR_log2` / `FR_ln` / `FR_log10`:
  new leading-bit-position + 33-entry mantissa table with linear
  interpolation. v1 returned wrong values for non-power-of-2
  inputs.
- `FR_pow2` / `FR_exp` / `FR_pow10`:
  corrected floor-toward-−∞ integer-part extraction.
  v1 gave wrong answers for negative non-integer inputs.
- `FR_atan2`: v1 was a placeholder that returned
  garbage. v2 is a correct octant-reduced arctan with max error
  ≤ 1°. The vestigial `radix` argument was
  dropped.
- `FR_atan`, `FR_Tan`, `FR_TanI`:
  wiring and overflow fixes.
- `FR_printNumD/F/H`: fixed undefined behaviour on
  `INT_MIN` and a broken fraction extraction in the
  v1 code.
- `FR_DEG2RAD` / `FR_RAD2DEG`: macro bodies
  were swapped in v1. v2 fixes them and adds missing parentheses.

### New functionality

- **Radian-native trig**: `fr_sin`,
  `fr_cos`, `fr_tan`,
  `fr_sin_bam`, `fr_cos_bam`,
  `fr_sin_deg`, `fr_cos_deg`. Uses a new
  129-entry quadrant cosine table with round-to-nearest linear
  interpolation. Max error ≤ 1 LSB of s0.15 (~3e−5).
- **BAM macros**: `FR_DEG2BAM`,
  `FR_BAM2DEG`, `FR_RAD2BAM`,
  `FR_BAM2RAD`. BAM (16 bits per full circle) is the
  natural integer representation for phase accumulators and
  gives zero quantisation at the wraparound.
- **Square root and hypot**: `FR_sqrt`
  uses a digit-by-digit integer isqrt on `int64_t`;
  `FR_hypot` computes `sqrt(x² + y²)`
  with no intermediate overflow across the full `s32`
  range.
- **Wave generator family**: `fr_wave_sqr`,
  `fr_wave_pwm`, `fr_wave_tri`,
  `fr_wave_saw`, `fr_wave_tri_morph`,
  `fr_wave_noise`. All are stateless and take a
  `u16` BAM phase. Suitable for embedded audio, LFOs,
  and control signals.
- **ADSR envelope generator**: linear-segment
  attack/decay/sustain/release with s1.30 internal state so very
  long durations at high sample rates still have non-zero
  per-sample increments.
- **Compile-time table size**:
  `-DFR_TRIG_TABLE_BITS=8` doubles table precision at
  the cost of a larger (257-entry) table. Default is 7 / 129
  entries.

### New documentation

- [API Reference](api-reference.md) —
  complete per-symbol reference with inputs, outputs, radix
  handling, and worst-case error.
- `CONTRIBUTING.md` — PR expectations,
  portability rules, commit format.
- `tools/interp_analysis.html` — interactive
  Chart.js comparison of seven interpolation methods on the trig
  table.
- This documentation site (the one you're reading).

### Breaking changes

- `FR_NUM(i, f, r)` → `FR_NUM(i, f, d, r)`. The v1 form was broken (it ignored `f`
  entirely), so any caller was already getting wrong results.
- `FR_atan2(y, x, radix)` → `FR_atan2(y, x)`. The `radix` parameter was vestigial.
- `FR_RESULT` and the `FR_E_*` codes are
  gone. `inv()` returns `bool`;
  `add()` / `sub()` /
  `setrotate()` return `void`. Math
  functions use named sentinels `FR_DOMAIN_ERROR`,
  `FR_OVERFLOW_POS`, `FR_OVERFLOW_NEG`.
- `FR_SQUARE` and `FR_FIXMUL32u` removed
  — use `FR_FixMuls` / `FR_FixMulSat`,
  which now have an `int64_t` fast path and work at any
  radix.
- `FR_NO_INT64` and `FR_NO_STDINT` build
  flags removed. Every modern C99 toolchain ships
  `<stdint.h>` and 64-bit arithmetic.

### Test suite

v2 ships with **42 tests** across six test binaries
and a characterisation suite (`test_tdd.cpp`) that pins
numerical behaviour to bit-exact reference values. Overall line
coverage is **99%** on the library sources.

## v1.0.3 — 2025

Test-coverage release. Overall line coverage went from 4% to 72%
with four new test files
(`test_comprehensive.c`,
`test_overflow_saturation.c`,
`test_full_coverage.c`,
`test_2d_complete.cpp`). Added a GitHub Actions CI with
multi-platform (Linux, macOS) and multi-compiler (gcc, clang)
matrices, cross-compilation for ARM and RISC-V, and 32-bit
compatibility testing.

Also fixed a broken `FR_atan` implementation, corrected
`FR_PI` / `FR_2PI` / `FR_E`
declarations to use `const`, and fixed
`XFormPtI` test assumptions.

## v1.02 — first public release

Basic fixed-point operations, 2D transforms, and integer-degree
trigonometry. This is the version that shipped inside the original
Palm Pilot Inkstorm application.

## v1.01 — internal development

Naming conventions cleanup and an initial test framework. Never
released publicly.

## Timeline

FR_Math has been in continuous service since **2000**,
when it was written to run 2D graphics transforms on 16 MHz 68k
Palm Pilots for Trumpetsoft's *Inkstorm*. It has since
been ported to ARM, x86, MIPS, RISC-V, and a menagerie of 8- and
16-bit embedded targets. v2.0.0 is the first major revision with a
full test suite, a bit-exact numerical specification, and CI on
every push.
