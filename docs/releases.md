# Releases

Release highlights. For the full per-symbol change log, see
[release_notes.md](https://github.com/deftio/fr_math/blob/master/release_notes.md)
in the repo.

## v2.0.1 — 2026

Precision and accuracy release. All changes are backward-compatible
with v2.0.0 except where noted.

### Trig output precision

- `fr_cos_bam` / `fr_sin_bam` now return **s15.16** (was s0.15).
  Exact values at cardinal angles: `cos(0°) = 65536`, `cos(90°) = 0`.
  `FR_TRIG_ONE = 65536`.
- `FR_TanI` / `fr_tan` return s15.16 (was s16.15). Saturation
  is now `±INT32_MAX`.
- All wrappers updated: `FR_Cos`, `FR_Sin`, `FR_CosI`, `FR_SinI`,
  `FR_TanI`, `FR_Tan`.

### Inverse trig now returns radians

- `FR_acos`, `FR_asin`, `FR_atan` gain an `out_radix` parameter
  and return radians at that radix (was degrees as `s16`).
- `FR_atan2(y, x, out_radix)` also returns radians.
- `FR_BAM2RAD` macro corrected (was off by a factor of 1024).

### Rounding improvements

- `FR_FixMuls` / `FR_FixMulSat`: add 0.5 LSB (`+0x8000`) before
  the `>>16` shift. Both now **round to nearest** instead of
  truncating.
- `FR_sqrt` / `FR_hypot`: the internal `fr_isqrt64` now rounds to
  nearest (remainder > root → +1). Worst-case error drops from
  <1 LSB to **≤ 0.5 LSB**.

### Improved exp / log accuracy

- **`FR_pow2` table expanded** from 17 entries (16 segments) to
  65 entries (64 segments, 260 bytes). Interpolation error drops
  by ~16×.
- **`FR_MULK28` macro** added: multiplies any fixed-point value
  by a radix-28 constant using a 64-bit intermediate with
  round-to-nearest. ~9 decimal digits of precision in the
  constant.
- `FR_EXP` and `FR_POW10` now use `FR_MULK28` for the base
  conversion instead of shift-only macros. Much lower error.
- `FR_ln` and `FR_log10` also use `FR_MULK28` internally.
- **`FR_EXP_FAST`** and **`FR_POW10_FAST`** added as shift-only
  alternatives for 8-bit targets where 64-bit multiply is
  expensive.
- Four radix-28 constants added: `FR_kLOG2E_28`,
  `FR_krLOG2E_28`, `FR_kLOG2_10_28`, `FR_krLOG2_10_28`.

### New utility macros

- `FR_MIN`, `FR_MAX`, `FR_CLAMP` — standard min/max/clamp.
- `FR_DIV(x, xr, y, yr)` — fixed-point division with 64-bit
  pre-scaling. `FR_DIV32` is the 32-bit-only legacy path.
- `FR_MOD(x, xr, y, yr)` — fixed-point modulus.

### Infrastructure

- Docker cross-compile size report (9 targets).
- `sync_version.sh` rewrite — `FR_MATH_VERSION_HEX` is the
  single source of truth for version numbers.
- CI auto-release job, Dependabot config.
- Documentation audit: 14 errors fixed across header, source,
  markdown, and HTML docs.

### Breaking changes from v2.0.0

| Change | v2.0.0 | v2.0.1 |
|--------|--------|--------|
| sin/cos return type | `s16` (s0.15) | `s32` (s15.16) |
| sin/cos 1.0 value | 32767 | 65536 (exact) |
| tan return format | s16.15 (radix 15) | s15.16 (radix 16) |
| tan saturation | `±(32767 << 15)` | `±INT32_MAX` |
| FR_acos/asin signature | `(input, radix)` → s16 degrees | `(input, radix, out_radix)` → s32 radians |
| FR_atan signature | `(input, radix)` → s16 degrees | `(input, radix, out_radix)` → s32 radians |
| FR_atan2 signature | `(y, x)` → s16 degrees | `(y, x, out_radix)` → s32 radians |
| FR_BAM2RAD | off by 1024× (bug) | correct |

---

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
- `FR_pow2` / `FR_EXP` / `FR_POW10`:
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
