# FR_Math Precision, Units, and Saturation Reference

This document is the authoritative per-symbol reference for every public
macro, constant, and function in FR_Math 2.x. For each entry it lists:

- **Inputs** — types, units, valid ranges, side-effect notes (for macros)
- **Output** — type, units, format (`sM.N` notation)
- **Precision** — worst-case absolute error, bias, and noise floor
- **Saturation** — whether the function clamps or wraps on overflow
- **Notes** — gotchas, v1→v2 changes, recommended alternatives

## Table of contents

1. [Notation and conventions](#1-notation-and-conventions)
2. [Types (`FR_defs.h`)](#2-types-fr_defsh)
3. [Core radix macros](#3-core-radix-macros)
4. [Arithmetic macros and functions](#4-arithmetic-macros-and-functions)
5. [Constants](#5-constants)
6. [Angle conversion macros](#6-angle-conversion-macros)
7. [Trig (v2 radian-native)](#7-trig-v2-radian-native)
8. [Trig (v1 integer-degree)](#8-trig-v1-integer-degree)
9. [Inverse trig](#9-inverse-trig)
10. [Logarithms and powers](#10-logarithms-and-powers)
11. [Print helpers](#11-print-helpers)
12. [2D matrix (`FR_math_2D.h`)](#12-2d-matrix-fr_math_2dh)

---

## 1. Notation and conventions

- **`sM.N`** — signed fixed-point with `M` integer bits and `N` fractional
  bits. Total storage is `M + N + 1` bits (the +1 for the sign). A value
  stored as `sM.N` represents the real number `stored_int / (1 << N)`.
- **`uM.N`** — same but unsigned (no sign bit).
- **LSB** — least-significant-bit weight. For `sM.N` the LSB weight is
  `1 / (1 << N)`. For s0.15 the LSB is `1/32767 ≈ 3.05e-5`.
- **radix** — the `N` in `sM.N`; the number of fractional bits. Nearly
  every function takes `radix` as an argument so the same binary can
  operate at any precision.
- **"Saturates"** means the result clamps to the representable extreme on
  overflow. "Wraps" means two's-complement wraparound. "UB on overflow"
  means the function assumes you sized your radix such that overflow
  cannot happen, and behavior is undefined otherwise.
- **"Side-effect note"** on a macro means the argument is evaluated more
  than once; do not pass expressions with side effects like `x++` or
  `f()`.

---

## 2. Types (`FR_defs.h`)

On toolchains with `<stdint.h>` (default), these are aliases for the C99
fixed-width types:

| Alias | Underlying | Range |
|---|---|---|
| `s8`  | `int8_t`   | −128 … 127 |
| `u8`  | `uint8_t`  | 0 … 255 |
| `s16` | `int16_t`  | −32768 … 32767 |
| `u16` | `uint16_t` | 0 … 65535 |
| `s32` | `int32_t`  | −2147483648 … 2147483647 |
| `u32` | `uint32_t` | 0 … 4294967295 |

`<stdint.h>` is mandatory in v2. Any C99-or-newer toolchain (gcc, clang,
MSVC, IAR, Keil, sdcc, MSP430-gcc, AVR-gcc, RISC-V toolchains, ARM
toolchains) supports it. If your toolchain predates C99, FR_Math 1.0.x
is the version for you.

v1→v2 change: in v1, `s32` was `signed long` and therefore 64 bits on
LP64 platforms, which silently broke every fixed-point computation that
assumed 32 bits. v2 uses `int32_t` so `s32` is guaranteed 4 bytes.

---

## 3. Core radix macros

All of these are macros. Arguments in `x` are evaluated once; `r`/`xr`/`yr`
are evaluated once each. None of them saturate; they rely on the programmer
to leave enough headroom.

### `I2FR(x, r)` — integer → fixed-radix
- **Input**: `x` any signed int, `r` radix (0 … 31)
- **Output**: `x << r`, same signedness
- **Precision**: exact (no rounding)
- **Saturation**: none (wraps on overflow)
- **Notes**: equivalent to `(x * (1<<r))`. Undefined if `x << r` overflows
  the integer type.

### `FR2I(x, r)` — fixed-radix → integer, truncating
- **Input**: `x` signed fixed-radix, `r` radix
- **Output**: `x >> r`, truncated toward −∞ (arithmetic shift on most
  platforms)
- **Precision**: truncation error in `[-1 LSB, 0]` relative to the real
  value

### `FR_INT(x, r)` — fixed-radix → integer, truncating toward zero
- **Input**: `x` signed fixed-radix, `r` radix
- **Output**: integer part of `x`, always toward zero (unlike `FR2I`)
- **Precision**: error in `[0, +1 LSB]` for negatives, `[0, +1 LSB]` for
  positives (symmetric around zero)

### `FR_NUM(i, f, d, r)` — literal builder
- **Input**: `i` integer part, `f` decimal digits of fractional part, `d`
  number of decimal digits written, `r` output radix
- **Example**: `FR_NUM(12, 34, 2, 10)` ≈ 12.34 in s.10 = 12636
- **Output**: s32 at radix `r`
- **Precision**: the fractional conversion is truncated toward zero at
  the call site. For round-to-nearest, add half an LSB at the caller.
- **Side-effect note**: evaluates each arg once
- **v1→v2 change**: the v1 signature was `FR_NUM(i, f, r)` and the body
  ignored `f` entirely. v2 adds the required `d` digit count. **Callers
  of the v1 form must be updated.**

### `FR_CHRDX(x, r_cur, r_new)` — change radix
- **Input**: `x` at current radix, source/destination radixes
- **Output**: `x` shifted to the new radix
- **Precision**: exact if `r_new ≥ r_cur`; truncates toward −∞ otherwise
- **Saturation**: none

### `FR_FRAC(x, r)` — fractional part, magnitude
- **Input**: `x` signed fixed-radix, `r` radix
- **Output**: `|x| mod (1<<r)` — the absolute fractional part
- **Notes**: equivalent to masking off the integer bits of `|x|`.
- **Side-effect note**: evaluates `x` twice (via `FR_ABS`)

### `FR_FRACS(x, xr, nr)` — fractional part scaled to new radix
- Composition of `FR_FRAC` then `FR_CHRDX`

### `FR_ADD(x, xr, y, yr)` and `FR_SUB(x, xr, y, yr)`
- **Input**: `x` at radix `xr`, `y` at radix `yr`
- **Behavior**: in-place, `x += FR_CHRDX(y, yr, xr)`
- **Precision**: if `yr > xr`, the lower (yr−xr) bits of `y` are
  truncated before adding, giving up to 1 LSB error at `xr`
- **Saturation**: none (wraps). Use `FR_FixAddSat` for clamped add.
- **Not commutative**: `FR_ADD(i,ir,j,jr) != FR_ADD(j,jr,i,ir)` because
  the result radix is always the first operand's radix.

### `FR_ISPOW2(x)`
- **Input**: integer
- **Output**: non-zero if `x` is a power of two, zero otherwise
- **Notes**: returns non-zero for `x = 0` (since `0 & -1 == 0`). Guard
  with `x != 0` if that matters.

### `FR_FLOOR(x, r)` and `FR_CEIL(x, r)`
- **Input**: signed fixed-radix
- **Output**: integer part (in the same radix) toward −∞ / +∞
  respectively
- **Precision**: exact
- **Side-effect note**: `FR_CEIL` evaluates `x` four times

### `FR_INTERP(x0, x1, delta, prec)` — linear interpolation
- **Input**: `x0`, `x1` any radix (same as each other); `delta` in
  `[0, 1<<prec]`; `prec` the precision of `delta`
- **Output**: linear interpolant at the same radix as `x0`/`x1`
- **Precision**: truncating `>>`, ≤ 1 LSB error of the output radix
- **Saturation**: none. `delta` outside `[0, 1<<prec]` performs
  extrapolation; watch for overflow.
- **Side-effect note**: evaluates `x0`, `x1`, `delta` twice each

### `FR_INTERPI(x0, x1, delta, prec)`
- Like `FR_INTERP` but masks `delta` to `[0, (1<<prec)-1]`, ensuring
  no extrapolation. Delta must be non-negative.

### `FR2D(x, r)` / `D2FR(d, r)` — double conversion (debug only)
- These pull in floating-point. Do not call them from production
  firmware unless you have an FPU.

### `FR_ABS(x)`, `FR_SGN(x)`
- **`FR_ABS`**: absolute value. **Side-effect note**: evaluates `x`
  twice. **Overflow**: `FR_ABS(INT_MIN)` is undefined behavior.
- **`FR_SGN`**: arithmetic sign, returns 0 for non-negative and −1 for
  negative (not +1/−1 — this is a bit-level sign extension).

---

## 4. Arithmetic macros and functions

### `FR_FixMuls(s32 x, s32 y)` — signed fixed-point multiply
- **Input**: two `s32` values at s16.16 (or any radix, as long as the
  result fits)
- **Output**: `(x * y) >> 16` as `s32`
- **Precision**: exact (`int64_t` intermediate)
- **Saturation**: **none**; wraps on overflow

### `FR_FixMulSat(s32 x, s32 y)` — signed saturating multiply
- **Input**: two `s32` values at s16.16
- **Output**: `clip((x*y)>>16, FR_OVERFLOW_NEG, FR_OVERFLOW_POS)`
- **Precision**: exact (`int64_t` intermediate)
- **Saturation**: **yes**, to `s32` extremes

### `FR_FixAddSat(s32 x, s32 y)` — saturating add
- **Input**: two `s32` at any common radix
- **Output**: `clip(x + y, INT32_MIN, INT32_MAX)`
- **Precision**: exact
- **Saturation**: yes

---

## 5. Constants

All constants are `s16.16` unless noted. They are `#define`d integer
literals, so they have no runtime cost and no precision other than the
rounding applied when they were computed.

| Macro | Value (approx) | Represents | s16.16 LSBs of error |
|---|---|---|---|
| `FR_kPREC` | 16 | radix of all constants | — |
| `FR_kE`         | 178145  | e = 2.71828… | 0.2 |
| `FR_krE`        | 24109   | 1/e = 0.36788… | 0.1 |
| `FR_kPI`        | 205887  | π = 3.14159… | 0.2 |
| `FR_krPI`       | 20861   | 1/π = 0.31831… | 0.1 |
| `FR_kDEG2RAD`   | 1144    | π/180 = 0.01745… | 0.1 |
| `FR_kRAD2DEG`   | 3754936 | 180/π = 57.2958… | 0.1 |
| `FR_kQ2RAD`     | 102944  | π/2 = 1.57080… | 0.1 |
| `FR_kRAD2Q`     | 41722   | 2/π = 0.63662… | 0.1 |
| `FR_kLOG2E`     | 94548   | log₂(e) = 1.44269… | 0.1 |
| `FR_krLOG2E`    | 45426   | ln(2) = 0.69315… | 0.1 |
| `FR_kLOG2_10`   | 217706  | log₂(10) = 3.32193… | 0.1 |
| `FR_krLOG2_10`  | 19728   | log₁₀(2) = 0.30103… | 0.1 |
| `FR_kSQRT2`     | 92682   | √2 = 1.41421… | 0.1 |
| `FR_krSQRT2`    | 46341   | 1/√2 = 0.70711… | 0.1 |
| `FR_kSQRT3`     | 113512  | √3 = 1.73205… | 0.1 |
| `FR_krSQRT3`    | 37837   | 1/√3 = 0.57735… | 0.1 |
| `FR_kSQRT5`     | 146543  | √5 = 2.23607… | 0.1 |
| `FR_krSQRT5`    | 29309   | 1/√5 = 0.44721… | 0.1 |
| `FR_kSQRT10`    | 207243  | √10 = 3.16228… | 0.1 |
| `FR_krSQRT10`   | 20724   | 1/√10 = 0.31623… | 0.1 |

Trig internal constants:

| Macro | Value | Represents |
|---|---|---|
| `FR_TRIG_PREC`   | 15    | output radix of `FR_Sin`/`FR_Cos` |
| `FR_TRIG_MASK`   | 32767 | `(1 << FR_TRIG_PREC) - 1` |
| `FR_TRIG_MAXVAL` | 32767 | returned by `FR_TanI(90°)` etc. |
| `FR_TRIG_MINVAL` | −32767 | returned by `FR_TanI(270°)` etc. |
| `FR_LOG2MIN`     | −2147418112 | returned when input to log is 0 |

---

## 6. Angle conversion macros

### Shift-only macros (pure bit shifts, no multiply)

These are designed for tiny MCUs without a multiplier. They trade a few
LSBs of precision for no multiplication and minimal ROM.

| Macro | Conversion | Worst-case relative error | Side-effect evals |
|---|---|---|---|
| `FR_DEG2RAD(x)` | deg → rad at same radix | ~1.6e−4 | 3× |
| `FR_RAD2DEG(x)` | rad → deg at same radix | ~2.1e−6 | 7× |
| `FR_RAD2Q(x)`   | rad → quadrants | ~2e−5 | 5× |
| `FR_Q2RAD(x)`   | quadrants → rad | ~2e−5 | 5× |
| `FR_DEG2Q(x)`   | deg → quadrants | ~1e−4 | 4× |
| `FR_Q2DEG(x)`   | quadrants → deg | exact (multiply by 90) | 4× |

**v1→v2 change**: `FR_DEG2RAD` and `FR_RAD2DEG` had their bodies swapped
in v1 (so `FR_DEG2RAD` actually multiplied by 57.3 and vice versa).
v2 fixes this. `FR_DEG2RAD` also had a missing paren around `x` in one
sub-expression, now fixed.

**Notes**: none of these saturate. For large-magnitude inputs the
high-bit shifts can overflow; use the equivalent multiply-based path
(`x * FR_kDEG2RAD >> FR_kPREC`) if you need the full `s32` range.

### BAM (Binary Angular Measure)

BAM is the v2 internal angle representation: 16 bits for a full circle
(so a `u16` IS a BAM). The top 2 bits are the quadrant, the next 7 bits
index the 129-entry trig table, and the bottom 7 bits drive linear
interpolation.

| Constant | Value | Meaning |
|---|---|---|
| `FR_BAM_BITS`     | 16 | bits in a full circle |
| `FR_BAM_FULL`     | 65536 | one full revolution |
| `FR_BAM_QUADRANT` | 16384 | 90° / π/2 rad |
| `FR_BAM_HALF`     | 32768 | 180° / π rad |

| Macro | Conversion | Precision | Saturates? | Side-effect evals |
|---|---|---|---|---|
| `FR_DEG2BAM(deg)` | integer degrees → BAM | ≤ 0.5 LSB BAM (~0.0028°), no accumulation | wraps (correct) | 2× |
| `FR_BAM2DEG(bam)` | BAM → integer degrees | truncated, ≤ 0.5° per step | — | 1× |
| `FR_RAD2BAM(rad, radix)` | rad at `radix` → BAM | ~0.004% (≈ 3 LSB at 2π) | wraps | 1× |
| `FR_BAM2RAD(bam, radix)` | BAM → rad at `radix` | ~0.02% | — | 1× |

**v1→v2 change**: `FR_DEG2BAM` originally used `(deg * 182) + (deg * 11 >> 5)`
which accumulated ~0.3 LSB per degree and reached ~108 LSB error at 360°.
v2 uses `(deg << 16) / 360` with sign-aware rounding — constant ≤ 0.5 LSB
regardless of input magnitude. See `dev/fr_math2_impl_plan.md` §Phase 2.

---

## 7. Trig (v2 radian-native)

All v2 trig functions go through a 129-entry s0.15 quadrant cosine table
in `src/FR_trig_table.h` and use linear interpolation with round-to-nearest.

**Precision budget** (all functions in this section):

| Source | Contribution |
|---|---|
| Table quantization | ≤ 0.5 LSB s0.15 |
| Linear interp truncation (fix in v2: rounded) | ≤ 0.5 LSB s0.15 |
| Sum | **≤ 1 LSB s0.15 worst case** (~3e−5 absolute) |
| Mean error | **~0** (no bias; the round-to-nearest removed the v1 negative bias) |

**Table size knob**: set `-DFR_TRIG_TABLE_BITS=8` (and regenerate the
table with `tools/coef-gen.py`) to get a 257-entry table, which cuts
max error to ≈ 0.25 LSB. Default is 7 (129 entries).

### `s16 fr_cos_bam(u16 bam)` — cos from BAM
- **Input**: BAM angle, full range
- **Output**: s0.15, range `[-32767, +32767]`
- **Precision**: ≤ 1 LSB; no bias
- **Saturation**: output is always in range (no overflow)
- **Cycles (approx)**: 1 multiply + 2 shifts + 1 branch + 2 array loads
- **Notes**: the inner loop uses the "subtract form"
  `lo - ((d*frac + 64) >> 7)` where `d = lo − hi ≥ 0`, so the `>>` only
  ever sees non-negative operands — fully portable C89, no reliance on
  implementation-defined behavior of right-shift on negatives.

### `s16 fr_sin_bam(u16 bam)` — sin from BAM
- Implemented as `fr_cos_bam(bam − FR_BAM_QUADRANT)`. The `u16`
  wraparound handles negative arguments for free.
- Same precision and cost as `fr_cos_bam`.

### `s16 fr_cos(s32 rad, u16 radix)` / `s16 fr_sin(s32 rad, u16 radix)`
- **Input**: `rad` at the given `radix` (e.g. s16.16 is `radix = 16`),
  any `s32` value
- **Output**: s0.15
- **Precision**: same as `fr_cos_bam`, plus ~0.5 BAM LSB of input
  quantization from the rad→BAM conversion (≈ 1.5 total LSBs worst case)
- **Saturation**: output clamped (sin/cos are intrinsically bounded);
  radix conversion uses `int64_t` to avoid overflow
- **Notes**: for extreme inputs (|rad| > 2^16 at radix 16), the BAM
  wraparound is still mathematically correct — BAM naturally handles
  modular arithmetic.

### `s32 fr_tan(s32 rad, u16 radix)`
- **Input**: `rad` at given `radix`
- **Output**: `s15.16` (tan can exceed 1)
- **Precision**: dominated by the divide; near singularities (cos ≈ 0)
  the relative error blows up. For |cos| > 0.1 the error is ≤ 10 LSBs
  of s15.16.
- **Saturation**: **yes**. When `cos == 0` exactly, returns
  `±FR_TRIG_MAXVAL << FR_TRIG_PREC` based on the sign of sin.
- **Notes**: avoid inputs near ±π/2 + kπ unless you need the saturated
  behavior.

### `fr_cos_deg(deg)` / `fr_sin_deg(deg)` — integer-degree macros
- Wrappers: `fr_cos_bam(FR_DEG2BAM(deg))`.
- **Side-effect note**: `deg` is referenced twice by `FR_DEG2BAM`.

---

## 8. Trig (v1 integer-degree)

Kept for source-level compatibility. **New code should use the v2 names
(lowercase) from section 7.**

| Function | Input | Output | Precision |
|---|---|---|---|
| `FR_CosI(s16 deg)`  | integer deg | s0.15 | ≤ 1 LSB |
| `FR_SinI(s16 deg)`  | integer deg | s0.15 | ≤ 1 LSB |
| `FR_Cos(s16 deg, u16 radix)`  | fixed-radix deg | s0.15 | ≤ 1 LSB |
| `FR_Sin(s16 deg, u16 radix)`  | fixed-radix deg | s0.15 | ≤ 1 LSB |
| `FR_TanI(s16 deg)`  | integer deg | s15.16 | ≤ 10 LSB (away from singularity) |
| `FR_Tan(s16 deg, u16 radix)` | fixed-radix deg | s15.16 | ≤ 10 LSB (away from singularity) |

**Saturation**: `FR_TanI` / `FR_Tan` return `±FR_TRIG_MAXVAL << FR_TRIG_PREC`
at integer multiples of 90° (where tan is ±∞). `FR_CosI` / `FR_SinI` are
always in range.

**v1→v2 change**: `FR_Tan` used `s16` loop variables that overflowed for
angles near 90°. v2 uses `s32`. `FR_TanI` had dead `if (270 == deg)` code
that was unreachable; removed in v2.

---

## 9. Inverse trig

All inverse trig functions output **integer degrees** (not radians, not
BAM) for human readability. Multiply by `FR_kDEG2RAD >> FR_kPREC` to get
radians if you need them.

### `s16 FR_atan2(s32 y, s32 x, u16 radix)`
- **Input**: `y`, `x` as a ratio, both at the same `radix`. Each in
  `s31 − radix` range.
- **Output**: angle in integer degrees, range `[-180, 180]`
- **Precision**: ≤ 1° worst case (uses a 33-entry arctan table with
  linear interpolation and octant reduction)
- **Saturation**: handles `(0, 0)` by returning 0. Axis cases
  (`x == 0` or `y == 0`) are handled exactly.
- **v1→v2 change**: v1 was a placeholder that returned garbage. v2 is
  a correct octant-reduced arctan.

### `s16 FR_atan(s32 input, u16 radix)`
- **Input**: `input` at given `radix`, interpreted as the tangent of
  the unknown angle
- **Output**: integer degrees, range `[-90, 90]`
- **Precision**: ≤ 1°
- **Notes**: implemented as `FR_atan2(input, 1 << radix, radix)`

### `s16 FR_acos(s32 input, u16 radix)`
- **Input**: `input` at given `radix`, must be in `[-1.0, +1.0]` scaled
  (i.e. `[-(1<<radix), +(1<<radix)]`)
- **Output**: integer degrees in `[0, 180]`
- **Precision**: ≤ 1°
- **Saturation**: clamps `input` to the valid range before computing

### `s16 FR_asin(s32 input, u16 radix)`
- Like `FR_acos` but returns `[-90, 90]`.

---

## 10. Logarithms and powers

### `s32 FR_log2(s32 input, u16 radix, u16 output_radix)`
- **Input**: `input` at `radix`, must be > 0. `output_radix` is the
  fractional precision of the answer.
- **Output**: `log2(input)` at `output_radix`
- **Precision**: ≤ 2 LSBs of `output_radix` (uses a 33-entry mantissa
  table + leading-bit-position; interpolation with `int64_t` fast path)
- **Saturation**: for `input ≤ 0` returns `FR_LOG2MIN` as a sentinel
- **v1→v2 change**: v1 was missing its accumulator in the lookup step
  and returned wrong values for non-power-of-2 inputs. v2 is correct.

### `s32 FR_ln(s32 input, u16 radix, u16 output_radix)`
- `FR_log2(input, radix, output_radix) * FR_krLOG2E >> FR_kPREC`
- Same precision and saturation as `FR_log2`, plus ≤ 1 LSB from the
  multiply.

### `s32 FR_log10(s32 input, u16 radix, u16 output_radix)`
- `FR_log2(input, radix, output_radix) * FR_krLOG2_10 >> FR_kPREC`
- Same precision and saturation as `FR_log2`, plus ≤ 1 LSB from the
  multiply.

### `s32 FR_pow2(s32 input, u16 radix)`
- **Input**: `input` at `radix`, any value
- **Output**: `2^input` at `radix`
- **Precision**: ≤ 2 LSBs for fractional inputs (17-entry fraction
  table + linear interp). Integer-power-of-two inputs are exact.
- **Saturation**: **yes** — clamps to `INT32_MAX` on overflow (when
  the result would exceed the `s32` range at the given `radix`)
- **v1→v2 change**: v1 used C-style truncation toward zero to split
  off the integer exponent, which gave wrong answers for negative
  non-integer inputs (e.g. `pow2(−0.5)` returned ~2 instead of ~0.707).
  v2 uses mathematical floor (toward −∞).

### `FR_EXP(input, radix)` — macro for `e^input`
- Expands to `FR_pow2(FR_SLOG2E(input), radix)`.
- **Precision**: adds the shift-scale `FR_SLOG2E` error (~1e−5 relative)
  to `FR_pow2`.
- **Side-effect note**: evaluates `input` multiple times (see `FR_SLOG2E`)

### `FR_POW10(input, radix)` — macro for `10^input`
- Expands to `FR_pow2(FR_SLOG2_10(input), radix)`.
- **Precision**: adds `FR_SLOG2_10` error to `FR_pow2`.

---

## 11. Print helpers

Each of these takes `int (*f)(char)` as the first argument: a function
that receives each output character (e.g. `putchar`). All three return
the number of bytes written, or −1 on a null `f` pointer.

### `int FR_printNumD(int (*f)(char), int n, int pad)`
- **Input**: `n` integer to print, `pad` minimum field width (space-padded)
- **Output bytes**: up to 12 chars for an `s32` (`-2147483648` + sign)
- **Return**: byte count, or −1 on null `f`
- **v1→v2 change**: v1 invoked `-n` on `INT_MIN`, which is undefined
  behavior. v2 works in unsigned magnitude to avoid this. v1 also
  returned 0 always; v2 returns the real byte count.

### `int FR_printNumF(int (*f)(char), s32 n, int radix, int pad, int prec)`
- **Input**: `n` fixed-radix number, `radix` fractional bits of `n`,
  `pad` field width, `prec` number of decimal digits to print
- **Output**: decimal form like `-12.3400` (no exponent)
- **Precision**: exact up to `prec` digits; last printed digit is the
  result of repeated `× 10` with truncation (up to 0.5 LSB error at the
  last digit position)
- **v1→v2 change**: INT_MIN fix same as `FR_printNumD`. v1 fraction
  extraction was also mathematically wrong (printed bogus tail digits).

### `int FR_printNumH(int (*f)(char), int n, int showPrefix)`
- **Input**: `n` integer, `showPrefix` = nonzero to emit `0x`
- **Output**: hex in uppercase, always 8 digits for `s32`
- **v1→v2 change**: v1 shifted a signed negative int right, which was
  implementation-defined. v2 casts to unsigned first.

---

## 12. Square root, hypot, waves, and ADSR (v2 new)

All symbols in this section are new in v2.0. None existed in v1.

### `s32 FR_sqrt(s32 input, u16 radix)`
- **Input**: `input` at `radix`. Any non-negative value fits up to s32; for
  negative input the function returns `INT32_MIN` as a domain-error sentinel.
- **Output**: `floor(sqrt(input))` at `radix`
- **Algorithm**: digit-by-digit ("shift-and-subtract") integer square root
  on a `uint64_t` accumulator. No division. Deterministic 32-iteration
  cost. Computes `isqrt(input << radix)`, which gives an `r`-radix result.
- **Precision**: bit-exact floor; worst-case absolute error < 1 LSB at
  the requested `radix` (the floor truncation). Always non-negative,
  monotone in `input`.
- **Saturation**: `input < 0` returns `FR_DOMAIN_ERROR` (`INT32_MIN`).
  No other saturation paths because `(u32 input) << radix` always fits
  in u64 for radix ≤ 30.

### `s32 FR_hypot(s32 x, s32 y, u16 radix)`
- **Input**: `x`, `y` at `radix`. Both can be negative; sign is squared away.
- **Output**: `floor(sqrt(x*x + y*y))` at `radix`
- **Algorithm**: direct sum-of-squares on `int64_t`, then `isqrt64`. The
  squared sum is at radix `2*radix`, and `isqrt` of a `2r`-radix value
  yields an `r`-radix result, so no extra shifting is needed.
- **Precision**: bit-exact floor; worst-case < 1 LSB at `radix`.
- **Saturation**: cannot overflow because
  `(INT32_MAX^2) * 2 ≈ 2^63 < 2^64`.

### Wave generators

All wave generators take a `u16` BAM phase (a full cycle is `[0, 65535]`)
and return `s16` in s0.15 format `[-32767, +32767]`, matching the trig
amplitude convention used by `fr_cos_bam` / `fr_sin_bam`.

Use `FR_HZ2BAM_INC(hz, sample_rate)` to get a per-sample phase
increment for a target frequency.

### `s16 fr_wave_sqr(u16 phase)`
- 50%-duty square wave. `phase < 0x8000 → +32767`, else `-32767`.
- **Precision**: exact (single comparison).
- **Saturation**: not applicable.

### `s16 fr_wave_pwm(u16 phase, u16 duty)`
- Variable-duty pulse. `phase < duty → +32767`, else `-32767`.
- `duty=0` → always low; `duty=0x8000` → 50% (same as `fr_wave_sqr`);
  `duty=0xffff` → high almost everywhere.
- **Precision**: exact.

### `s16 fr_wave_tri(u16 phase)`
- Symmetric triangle. Four linear segments:
  - `[0, 0x4000)`: rising 0 → +peak
  - `[0x4000, 0x8000)`: falling +peak → 0
  - `[0x8000, 0xc000)`: descending 0 → −peak
  - `[0xc000, 0x10000)`: rising −peak → 0
- Peaks are clamped to ±32767 (the natural unclamped formula gives
  ±32768 at the exact peak BAM values 0x4000 and 0xc000).
- **Precision**: < 1 LSB s0.15 vs ideal triangle (~3e−5 absolute,
  measured over a full 65536-point sweep).
- **Saturation**: peak clamp at ±32767.

### `s16 fr_wave_saw(u16 phase)`
- Rising sawtooth. Linear ramp from −32767 (at `phase=0`) to +32767
  (at `phase=0xffff`), passing through 0 at `phase=0x8000`.
- The single boundary case `phase=0` (which would naturally produce
  −32768) is clamped to −32767.
- **Precision**: exact (single subtract + clamp).
- **Saturation**: lower-edge clamp.

### `s16 fr_wave_tri_morph(u16 phase, u16 break_point)`
- Variable-symmetry triangle. Returns `[0, 32767]` (unipolar — caller
  can re-bias for a bipolar form).
- `break_point` is the BAM phase at which the peak occurs:
  - `0x8000` → symmetric triangle (50/50)
  - `→ 0xffff` → rising sawtooth (instant fall)
  - `→ 0x0001` → falling sawtooth (instant rise)
  - `0` is treated as `1` to avoid divide-by-zero
- **Precision**: < 1 LSB s0.15 from the integer division.
- **Cost**: one 32-bit divide per sample. Cheap on Cortex-M3+; slow on
  8051 / MSP430. Pre-compute slopes if those targets matter.
- **Saturation**: peak clamp at +32767.

### `s16 fr_wave_noise(u32 *state)`
- LFSR-based pseudorandom noise. Caller maintains a `u32` state; pass
  its address. **Initial state must be non-zero** (zero is a fixed
  point). A common seed is `0xACE1u`.
- **Algorithm**: 32-bit Galois LFSR with maximal-period polynomial
  `0xD0000001`. Period = 2^32 − 1 samples.
- **Output**: full-range `s16` in `[-32767, +32767]`.
- **Quality**: white-ish; suitable for synth use. **Not** crypto secure;
  for FFT-flat noise consider layering two LFSRs.
- **Side effects**: advances `*state`. Returns 0 if `state` is `NULL`.

### `FR_HZ2BAM_INC(hz, sample_rate)` — macro
- Computes the per-sample BAM phase increment for a target frequency.
- Expands to `(u16)(((u32)(hz) * 65536UL) / (u32)(sample_rate))`.
- **Range**: meaningful for `hz < sample_rate / 2` (Nyquist). The macro
  does not enforce this — out-of-range frequencies alias.
- **Side-effect note**: `hz` and `sample_rate` are evaluated once each.
- **Example**: `FR_HZ2BAM_INC(440, 48000)` = 600 (implies 439.45 Hz; the
  ~0.55 Hz error is the standard quantization of a 16-bit phase
  accumulator at 48 kHz — for higher precision, use a 32-bit phase).

### ADSR envelope generator

Linear-segment Attack-Decay-Sustain-Release envelope. Caller-allocated
struct, no malloc, no global state. Internal level is held in s1.30 so
even very long durations have a non-zero per-sample increment.

#### `fr_adsr_t` struct
| Field | Type | Meaning |
|---|---|---|
| `state` | `u8` | One of `FR_ADSR_IDLE/ATTACK/DECAY/SUSTAIN/RELEASE` |
| `level` | `s32` | Current envelope value, s1.30 (peak = `1<<30`) |
| `sustain` | `s32` | Sustain target, s1.30 |
| `attack_inc` | `s32` | Per-sample increment during attack |
| `decay_dec` | `s32` | Per-sample decrement during decay |
| `release_dec` | `s32` | Per-sample decrement during release |

#### State constants
- `FR_ADSR_IDLE` = 0
- `FR_ADSR_ATTACK` = 1
- `FR_ADSR_DECAY` = 2
- `FR_ADSR_SUSTAIN` = 3
- `FR_ADSR_RELEASE` = 4

#### Functions

| Function | Inputs | Effect | Notes |
|---|---|---|---|
| `fr_adsr_init(env, atk, dec, sus, rel)` | u32 atk samples, u32 dec samples, s16 sus_s015, u32 rel samples | sets all rates, clears state to IDLE | sus is clamped to [0, 32767]; zero durations collapse to 1-step transitions |
| `fr_adsr_trigger(env)` | env | sets state = ATTACK, level = 0 | call on note-on |
| `fr_adsr_release(env)` | env | sets state = RELEASE | call on note-off |
| `fr_adsr_step(env)` | env | advances one sample, returns s0.15 in [0, 32767] | call once per audio sample |

- **Saturation**: self-clamping. `level` cannot escape `[0, 1<<30]`.
- **Null safety**: all entry points check `env == NULL` and no-op (or
  return 0 for `fr_adsr_step`).

---

## 13. 2D matrix (`FR_math_2D.h`)

### `struct FR_Matrix2D_CPT`
A 2×3 affine transformation matrix for 2D point transforms. The bottom
row is always `[0 0 1]` and is not stored.

| Field | Type | Meaning |
|---|---|---|
| `m00, m01, m02` | s32 | top row of the affine matrix at precision `radix` |
| `m10, m11, m12` | s32 | middle row |
| `radix` | u16 | fractional bits of all matrix entries |
| `fast`  | int | 1 if `m01==m10==0` (scale+translate only) — enables a 2-mul fast path |

### Default radix
`FR_MAT_DEFPREC` is 8. Most constructors and setters default to this.

### Member functions

| Method | Inputs | Output | Precision | Saturates |
|---|---|---|---|---|
| `ID()` | — | identity matrix at current `radix` | exact | — |
| `det()` | — | s32 determinant at `radix` (not `2*radix`) | ≤ 1 LSB | no |
| `inv()` / `inv(pInv)` | — / target matrix | bool: true on success, false if singular | depends on det | no |
| `add(pAdd)` / `sub(pSub)` | matrix pointer | void, in-place | exact if no overflow | no |
| `setrotate(deg)` | integer deg | void, in-place | uses `FR_CosI`/`FR_SinI`, ≤ 1 LSB | — |
| `setrotate(deg, radix)` | fixed-radix deg | void, in-place | ≤ 1 LSB | — |
| `XlateI(x, y [, r])` | integer translation | in-place | exact | no |
| `XlateRelativeI(x, y [, r])` | integer delta translation | in-place | exact | no |
| `XFormPtI(x, y, *xp, *yp [, r])` | s32 point, optional radix | s32 point | ≤ 1 LSB | no |
| `XFormPtI16(x, y, *xp, *yp)` | s16 point | s16 point | ≤ 1 LSB | **wraps** on overflow |
| `XFormPtINoTranslate(...)` | as above, skips translation | as above | as above | as above |
| `checkfast()` | — | updates `fast` flag | — | — |

**Notes on saturation**:
- Integer matrix ops (`XFormPtI`) do NOT saturate. It is the caller's
  responsibility to size `radix` so the multiply-add fits in `s32`.
- `XFormPtI16` uses `s32` intermediates but casts the final result to
  `s16`, which wraps on overflow. Caller must ensure the transformed
  point fits in `s16`.

**Performance**:
- The `fast` flag is set automatically by `checkfast()`. When true, the
  transform uses 2 multiplies instead of 4. All `XForm*` methods check
  the flag.

---

## Cross-references

- **Implementation plan and change log**: `dev/fr_math2_impl_plan.md`
- **TDD characterization suite**: `tests/test_tdd.cpp` — every entry
  in this document has a test case pinning its numerical behavior
- **Interpolation error analysis** (trig): `tools/interp_analysis.html`
- **Coefficient generator** (tables): `tools/coef-gen.py`

## Revision history

- **2.0.0** — Initial comprehensive precision reference, created
  alongside the v2 bug-fix pass. Documents the v1→v2 differences for
  every changed symbol.
