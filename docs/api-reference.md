# API Reference

Every public symbol, grouped by topic. Each entry lists the radix
convention, the precision, and the error / saturation behaviour. All
types are from `FR_defs.h`: `s8 s16 s32 s64` for
signed and `u8 u16 u32 u64` for unsigned integers (these are
aliases for the `<stdint.h>` types).

## Reading this reference

Most entries list **inputs**, **output**,
**radix handling** and **precision**
separately, because in a mixed-radix library those four things are
what actually lets you plan an arithmetic pipeline without hidden
quantisation. If you are new to fixed-point, the
[Fixed-Point Primer](fixed-point-primer.md) explains the
notation first; come back here once you're comfortable reading
`s15.16` and `s0.15`.

### Radix convention — the deliberate choice

FR_Math never stores the radix alongside the value. A fixed-point
number in this library is just an `s16` or `s32`;
the radix lives in the caller's head (or in a
`#define`). That sounds scary, but it's the
*reason* the library is useful on small cores:

- No per-value overhead — a million samples is still a
  million ints, not a million (int, radix) pairs.
- The compiler can turn every operation into a handful of shifts,
  adds and one multiply, because the shift amounts are known at
  compile time.
- You, the caller, pick the radix per call. That means you can run
  a hot loop at `s1.14`, then change radix to
  `s15.16` when you need range, using
  `FR_CHRDX`. No type conversion, no reallocation.

The rule of thumb is: every function that accepts a
`radix` argument interprets *all* of its fixed-point
inputs and outputs at that radix, unless the entry says otherwise.
When two functions in a pipeline use different radixes, use
`FR_CHRDX(value, from_r, to_r)` to rebase the value
explicitly — that is the single chokepoint for conversion and
the one place rounding happens.

A concrete example of the discipline:

```
/* Good: all three values live at radix 16 until we commit. */
s32 a   = I2FR(3, 16);                /* 3.0 at s15.16   */
s32 b   = FR_sqrt(I2FR(2, 16), 16);   /* sqrt(2) at s15.16 */
FR_ADD(a, 16, b, 16);                 /* a += b, both at radix 16  */

/* Commit back to an integer count at the end.
 * FR2I truncates; for round-to-nearest, add half an LSB first:
 *   a += (1 << (16 - 1));
 */
int n = FR2I(a, 16);
```

You never see a *radix* field on any of these values. The
shifts live in the macro parameters. That's the deal the
library makes with you: pay attention to the radix at each call site,
and in return get float-like ergonomics with integer-only codegen.

## Types and constants

### Integer type aliases

| Symbol | Meaning |
| --- | --- |
| `s8 s16 s32` | Signed integer typedefs (`int8_t`, `int16_t`, `int32_t`). |
| `u8 u16 u32` | Unsigned integer typedefs (`uint8_t`, `uint16_t`, `uint32_t`). |

### Sentinel return values (`FR_defs.h`)

| Symbol | Value | Used by |
| --- | --- | --- |
| `FR_OVERFLOW_POS` | `0x7FFFFFFF` (`INT32_MAX`) | Saturating ops when the true result exceeds `+2^31`. |
| `FR_OVERFLOW_NEG` | `0x80000000` (`INT32_MIN`) | Saturating ops when the true result is below `−2^31`. |
| `FR_DOMAIN_ERROR` | `0x80000000` (`INT32_MIN`) | Functions with an invalid input, e.g. `FR_sqrt(-1)`, `FR_log2(0)`, `FR_asin(2.0)`. **Shares the bit pattern of `FR_OVERFLOW_NEG`**, so don't mix a `≤ FR_OVERFLOW_NEG` check with a domain check — test for the exact sentinel. |

### Common numerical constants (`FR_math.h`)

All constants below are stored at radix 16 (s15.16). The prefix
`FR_k` means "constant"; `FR_kr`
means "reciprocal" (1/x). If you use them at a different
radix, change radix once with `FR_CHRDX` rather than
re-computing.

| Symbol | Value | Meaning |
| --- | --- | --- |
| `FR_kPREC` | 16 | The radix of every `FR_k*` constant in this file. |
| `FR_kE` | 178145 | *e* ≈ 2.71828. |
| `FR_krE` | 24109 | 1/*e* ≈ 0.36788. |
| `FR_kPI` | 205887 | π ≈ 3.14159. |
| `FR_krPI` | 20861 | 1/π ≈ 0.31831. |
| `FR_kDEG2RAD` | 1144 | π/180 ≈ 0.01745. |
| `FR_kRAD2DEG` | 3754936 | 180/π ≈ 57.29578. |
| `FR_kQ2RAD` | 102944 | π/2 ≈ 1.57080 (1 quadrant in radians). |
| `FR_kRAD2Q` | 41722 | 2/π ≈ 0.63662. |
| `FR_kLOG2E` | 94548 | log2(*e*) ≈ 1.44270. |
| `FR_krLOG2E` | 45426 | 1/log2(*e*) = ln(2) ≈ 0.69315. |
| `FR_kLOG2_10` | 217706 | log2(10) ≈ 3.32193. |
| `FR_krLOG2_10` | 19728 | 1/log2(10) = log10(2) ≈ 0.30103. |
| `FR_kSQRT2` | 92682 | √2 ≈ 1.41421. |
| `FR_krSQRT2` | 46341 | 1/√2 ≈ 0.70711. |

## Conversions and scalar macros

### Integer ↔ fixed-point

The bridge between a plain `int` and an FR_Math
fixed-point value is just a shift, but the library gives it names
so call sites read as intent:

| Macro | Inputs | Output | Effect |
| --- | --- | --- | --- |
| `I2FR(i, r)` | `i`: integer; `r`: target radix in bits | `s32` at radix `r` | `(i) << (r)`. No bounds check. Use when you know `|i|` fits in `32 − r` signed bits. |
| `FR2I(x, r)` | `x`: fixed-point at radix `r` | integer | `(x) >> (r)`. Truncates toward **−∞** (C's signed shift). `FR2I(-1, 4) == -1`, not 0. |
| `FR_INT(x, r)` | `x`: fixed-point at radix `r` | integer | Truncates toward **zero**. `FR_INT(-1, 4) == 0`. Useful when you want C's normal integer-cast behaviour. |
| `FR_NUM(i, f, d, r)` | `i`: integer part; `f`: decimal fraction digits; `d`: number of digits in `f`; `r`: target radix | `s32` at radix `r` | Build a fixed-point literal from decimal. `FR_NUM(12, 34, 2, 10)` is 12.34 at s.10. Rounds toward zero; for round-to-nearest, add half an LSB at the call site. |
| `FR_numstr(s, r)` | `s`: null-terminated decimal string (e.g. `"3.14159"`); `r`: target radix | `s32` at radix `r` | Runtime string-to-fixed-point parser (inverse of `FR_printNumF`). Handles signs, leading whitespace, and leading-zero fractions like `"0.05"`. Up to 9 fractional digits. No malloc, no strtod, no libm. Returns 0 for NULL or empty input. |
| `FR2D(x, r)` | `x`: fixed-point at radix `r` | `double` | Debug-only: `x / (double)(1 << r)`. Pulls in `libm` — compile it out of release builds. |
| `D2FR(d, r)` | `d`: `double`; `r`: target radix | `s32` at radix `r` | Debug-only: `(s32)(d * (1 << r))`. Same caveat as above. |

### Changing radix

| Macro | Inputs | Output | Notes |
| --- | --- | --- | --- |
| `FR_CHRDX(x, r_cur, r_new)` | `x`: fixed-point value at radix `r_cur` | same value at radix `r_new` | Expands to `(x) >> (r_cur-r_new)` if shrinking, or `(x) << (r_new-r_cur)` if growing. Growing is lossless; shrinking loses the bottom (`r_cur−r_new`) fractional bits. No rounding. |

### Fractional part, floor, ceiling

| Macro | Inputs | Output | Notes |
| --- | --- | --- | --- |
| `FR_FRAC(x, r)` | `x`: fixed-point at radix `r` | fractional part, at radix `r`, always ≥ 0 | Returns `|x| & ((1 << r) - 1)`. Loses the sign of `x`. |
| `FR_FRACS(x, xr, nr)` | `x`: at radix `xr`; `nr`: destination radix | fractional part at radix `nr` | Convenience wrapper: fractional part of `x`, rescaled to `nr`. |
| `FR_FLOOR(x, r)` | `x`: fixed-point at radix `r` | same value at radix `r` with fractional bits cleared | Integer part only, but the result stays at radix `r` so you can keep using it in the same pipeline. |
| `FR_CEIL(x, r)` | as above | as above | Same idea, rounds up. |
| `FR_ABS(x)` | any signed integer or fixed-point | same type, non-negative | Conditional: `((x) < 0 ? -(x) : (x))`. Evaluates `x` twice — don't pass an expression with side effects. |
| `FR_SGN(x)` | any signed integer or fixed-point | 0 or −1 | Arithmetic shift of the sign bit. Returns 0 for `x ≥ 0` and `−1` for `x < 0`. *Not* the classic `−1/0/+1` sign function. |
| `FR_ISPOW2(x)` | any integer | 0 or nonzero | Classic `x & (x-1)` trick. Returns nonzero when `x` is a power of two (including 0). |

### Interpolation

| Macro | Inputs | Output | Notes |
| --- | --- | --- | --- |
| `FR_INTERP(x0, x1, delta, prec)` | `x0`, `x1`: endpoints (any radix, same radix as each other); `delta`: blend at radix `prec` in [0, 1]; `prec`: radix of `delta` | Same radix as `x0`/`x1` | `x0 + ((x1 - x0) * delta) >> prec`. Linear lerp. Extrapolates outside `[0, 1]`; you own the overflow check. |
| `FR_INTERPI(x0, x1, delta, prec)` | as above, but `delta` is unsigned and masked into `[0, (1<<prec))` | as above | The "I" version forces `delta` into range, so it's safer when `delta` comes from an untrusted upstream (e.g. a running counter). |

## Utility macros

| Macro | Inputs | Output | Notes |
| --- | --- | --- | --- |
| `FR_MIN(a, b)` | Two values of the same type | The smaller of the two | Evaluates each argument once. |
| `FR_MAX(a, b)` | Two values of the same type | The larger of the two | Evaluates each argument once. |
| `FR_CLAMP(x, lo, hi)` | `x`: value; `lo`, `hi`: bounds | `x` clamped to `[lo, hi]` | Equivalent to `FR_MIN(FR_MAX(x, lo), hi)`. |
| `FR_DIV(x, xr, y, yr)` | `x`: numerator at radix `xr`; `y`: denominator at radix `yr` | `s32` at radix `xr` | Pre-scales the numerator in a 64-bit intermediate and **rounds to nearest** (adds half the divisor before truncating, with correct sign handling). Worst-case error ≤ 0.5 LSB. Works correctly across the full Q16.16 range. |
| `FR_DIV_TRUNC(x, xr, y, yr)` | same as `FR_DIV` | `s32` at radix `xr` | `((s64)(x) << (yr)) / (s32)(y)`. Truncating division (rounds toward zero). This was the behaviour of `FR_DIV` in v2.0.0; use it when you need exact backward compatibility or when the truncation bias is acceptable. |
| `FR_DIV32(x, xr, y, yr)` | same as `FR_DIV` | `s32` at radix `xr` | `((s32)(x) << (yr)) / (s32)(y)`. 32-bit-only truncating path — requires `|x| < 2^(31 − yr)` to avoid overflow in the intermediate shift. Use on tiny targets (PIC, AVR, 8051) where 64-bit ops pull in unwanted compiler runtime code. |
| `FR_MOD(x, y)` | `x`, `y`: same radix | remainder at the same radix | `(x) % (y)`. Standard C remainder semantics. |

## Arithmetic

FR_Math splits arithmetic into three flavours. The
**macros** (`FR_ADD`, `FR_SUB`)
are mixed-radix, inline, and wrap on overflow. The **s.16
helper functions** (`FR_FixMuls`,
`FR_FixMulSat`, `FR_FixAddSat`) are hardcoded
to radix 16, emit `int64_t` intermediates, and optionally
saturate. And the **scalar macros**
(`FR_ABS`, `FR_SGN`) listed above in the
conversions section handle sign.

### Mixed-radix add/subtract macros

**Important:** `FR_ADD` and
`FR_SUB` use *compound assignment*. They
**modify their first argument in place** — they
are not expressions you can chain in a C expression the way you'd
use `a + b`. Think of them as "`x += realign(y)`".

| Macro | Inputs | Output | Effect / precision |
| --- | --- | --- | --- |
| `FR_ADD(x, xr, y, yr)` | `x`: lvalue at radix `xr`; `y`: value at radix `yr` | `x` is updated in place, still at radix `xr` | `x += FR_CHRDX(y, yr, xr)`. If `yr > xr`, the bottom `yr − xr` bits of `y` are lost. **Wraps on overflow** — use a wider register or `FR_FixAddSat` when the sum can exceed range. |
| `FR_SUB(x, xr, y, yr)` | as above | as above | Same semantics with subtraction. Wraps on overflow. |

**Gotcha:**
`FR_ADD(i, ir, j, jr)` is not generally equal to
`FR_ADD(j, jr, i, ir)` — because the *first*
operand is the one whose radix the result keeps. The header explicitly
calls this out.

**Example:**

```
s32 a = I2FR(3, 4);    /* 3.0 at s.4  = 48  */
s32 b = I2FR(2, 8);    /* 2.0 at s.8  = 512 */

FR_ADD(a, 4, b, 8);    /* a now holds 5.0 at s.4 = 80                      */
                       /* b is shifted DOWN from radix 8 to radix 4 first, */
                       /* losing the bottom 4 fractional bits of b.        */
```

### Saturating radix-16 helpers

These are regular `s32`-returning functions (not
macros), and they are **hardcoded to radix 16**. Use
them for the common s15.16 pipeline. If you need a different radix,
use `FR_CHRDX` on the inputs and outputs, or build a
thin wrapper.

| Function | Inputs | Output | Precision / saturation |
| --- | --- | --- | --- |
| `s32 FR_FixMuls(s32 x, s32 y)` | `x`, `y`: s15.16 | `(x × y)` at s15.16 | Promotes to `int64_t`, adds 0.5 LSB (`+0x8000`), shifts right by 16. **Rounds to nearest.** **Wraps** on overflow — no clamp. Use when you have a formal bound on the product. |
| `s32 FR_FixMulSat(s32 x, s32 y)` | as above | as above | Same round-to-nearest, but clamps to `FR_OVERFLOW_POS` / `FR_OVERFLOW_NEG` on over/underflow. Prefer by default. |
| `s32 FR_FixAddSat(s32 x, s32 y)` | `x`, `y`: any signed s32 at the same radix (radix is not rescaled) | saturated sum at the same radix | Classic sign-watching saturating add: returns `FR_OVERFLOW_POS` or `FR_OVERFLOW_NEG` if adding two same-sign values would flip the sign. |

## Shift-only scaling macros

These macros exist specifically for CPUs without a hardware
multiplier (8051, low-end PICs, 68HC11, MSP430E, parts of
Cortex-M0+). Each one approximates a multiplication by a
floating-point constant using only shifts and adds. They evaluate
their argument multiple times, so don't pass side-effecting
expressions. Precision is stated per entry — if you need
better, use the multiply-by-`FR_kXXX` path and shift down
by `FR_kPREC` instead.

| Macro | Approximates | Relative error |
| --- | --- | --- |
| `FR_SMUL10(x)` | × 10 | exact (it's `(x << 3) + (x << 1)`). |
| `FR_SDIV10(x)` | ÷ 10 | ≈ 4e-4. |
| `FR_SLOG2E(x)` | × log2(*e*) (≈ 1.4427) | ≈ 3e-5, for converting `pow2 → exp`. |
| `FR_SrLOG2E(x)` | × ln(2) (≈ 0.6931) | ≈ 3e-5, for converting `log2 → ln`. |
| `FR_SLOG2_10(x)` | × log2(10) (≈ 3.3219) | ≈ 3e-5, for converting `pow2 → pow10`. |
| `FR_SrLOG2_10(x)` | × log10(2) (≈ 0.3010) | ≈ 3e-5, for converting `log2 → log10`. |
| `FR_DEG2RAD(x)` | × π/180 | ≈ 1.6e-4. |
| `FR_RAD2DEG(x)` | × 180/π | ≈ 2.1e-6. |
| `FR_RAD2Q(x)` | × 2/π (radians → quadrants) | small — used on the legacy quadrant path. |
| `FR_Q2RAD(x)` | × π/2 (quadrants → radians) | small. |

These are the "quadrant macros" from the v1 API. They
were written for chips where a 32-bit multiply was measured in
*dozens* of cycles and a shift was one cycle. They still have
value today on the smallest MCUs and any time you want an angle
conversion that can't accidentally overflow an intermediate.

## Angle conventions and BAM

FR_Math supports three ways to express an angle. Every trig
function in the library eventually reduces to one of these and funnels
through a single core routine, so the only reason there are multiple
APIs is *your* convenience.

- **Integer degrees** — `s16`,
  typically 0 ≤ θ < 360 (negative values are handled by the
  modular wrap). Used by the classic `FR_SinI` /
  `FR_CosI` / `FR_TanI` API; the trailing
  *I* denotes *integer* degrees.
- **Radians at a caller-chosen radix** —
  `s32` fixed-point. Used by the lowercase
  `fr_cos` / `fr_sin` / `fr_tan`
  family. The radix is a call-site parameter so you can trade range for
  precision per call.
- **BAM (Binary Angular Measure)** — a
  `u16` where the full circle maps to
  `[0, 65536)`. The top 2 bits select the quadrant, the next
  7 bits index the 128-entry quadrant cosine table, and the bottom 7
  bits drive linear interpolation. BAM is the native input of the core
  routine `fr_cos_bam` and every other wrapper converts to
  BAM before calling it.

### Why `u16` for BAM (not `s32`)?

The choice is deliberate. BAM is designed to be the state of a
phase accumulator, and `u16` gives you *modular
wraparound for free*. Every time you do
`phase += increment` on a `u16`, the result is
implicitly taken modulo 65536 — which is exactly one full
revolution. No glitch at the wrap, no explicit reduction step, no
conditional branch.

An `s32` would lose this property. You would have to
write `phase = (phase + increment) & 0xFFFF` on every
tick, the sign bit would be a trap, and the upper 16 bits would be
dead weight. 16 bits is also an exact match for the table's
useful resolution: 2 quadrant bits + 7 index bits + 7 interpolation
bits = 16. Going wider would only add noise, not precision.

"But what if I want to pass in any signed angle without worrying
about conversion?" That is exactly what `FR_CosI(deg)`,
`FR_Cos(deg, radix)`, and `fr_cos(rad, radix)` are for. All three
take *signed* inputs and reduce them to BAM for you. The only place
you actually see a `u16` is at the internal `fr_cos_bam` /
`fr_sin_bam` boundary, which you only call by hand if you *want*
a phase accumulator (and at that point, `u16` is the right type
anyway).

### Conversion macros

Four macros cover every conversion between the three
representations:

| Macro | Direction | Worst-case error |
| --- | --- | --- |
| `FR_DEG2BAM(deg)` | integer degrees → BAM | ≤ 0.5 LSB BAM (≈ 0.0028°), no accumulation. |
| `FR_BAM2DEG(bam)` | BAM → integer degrees | Truncation only (± 0.5°). |
| `FR_RAD2BAM(rad, r)` | radians (at radix `r`) → BAM | ≈ 0.036% (constant 10430 ≈ 65536 / 2π). |
| `FR_BAM2RAD(bam, r)` | BAM → radians (at radix `r`) | ≈ 0.006% (constant 6434 ≈ 2π × 1024). |

#### Worked example: keeping precision on chips without a multiplier

The constants in these macros are chosen so a small-MCU compiler
— or a human writing assembly — can drop back to pure
shift-and-add and lose no precision. This is a technique that
pre-dates hardware multipliers and may not be obvious to newer
programmers, so it's worth walking through.

Take `FR_BAM2DEG`:

```
#define FR_BAM2DEG(bam)  ((s16)(((s32)(u16)(bam) * 45) >> 13))
```

Why 45 and 13? Because the degrees-per-BAM ratio is
`360 / 65536 = 45 / 8192`, and 8192 is `1 << 13`.
So `bam * 45 >> 13` is an *exact* rational
multiply by the ratio, with one rounding step at the end. The
constant 45 is small and factors as
`32 + 8 + 4 + 1`, so on a chip with no multiply
instruction the compiler (or you) can expand it into four shifts and
three adds:

```
/* bam * 45 as shift-and-add: 45 = (1<<5) + (1<<3) + (1<<2) + 1 */
u32 x  = ((u32)bam << 5)
       + ((u32)bam << 3)
       + ((u32)bam << 2)
       +  (u32)bam;
s16 deg = (s16)(x >> 13);
```

Four shifts plus three adds — cheap on an 8051, AVR, or any
hand-written DSP inner loop — and the answer has at most
±0.5 LSB of truncation error. The same discipline applies to
the other direction: in `FR_DEG2BAM` the divide-by-360 is
a compile-time constant, so any optimising compiler folds it into a
multiply-by-reciprocal (or, on a weaker toolchain, a runtime call
that you can inline yourself).

**Takeaway:** whenever you see an "oddly
specific" constant in FR_Math — 45, 10430, 6434, 0xB505,
etc. — assume it was picked so the multiply either collapses to
shift-and-add on a small core, or matches a power-of-two shift on the
other side of the expression. It's not magic, it's just
rational arithmetic with the right denominator.

#### Using the macros

```
/* Sine of 42 degrees, result in s15.16. */
s32 y = fr_sin_bam(FR_DEG2BAM(42));

/* Phase accumulator at 440 Hz on a 48 kHz stream. */
u16 phase = 0;
u16 inc   = (u16)(((u32)440 << 16) / 48000u); /* BAM per sample  */
for (int n = 0; n < nsamples; ++n) {
    buf[n] = fr_sin_bam(phase);                /* s15.16 sine    */
    phase += inc;                              /* wraps at 65536 */
}

/* Radians (at radix 16) to BAM and back. */
s32 rad_16 = 1L << 16;            /* 1.0 rad */
u16 bam    = FR_RAD2BAM(rad_16, 16);
s32 back   = FR_BAM2RAD(bam, 16);
```

## Trigonometry

Every trig function in FR_Math — integer-degree, radian, or
BAM — funnels through a single 129-entry quadrant cosine table,
`gFR_COS_TAB_Q` (128 intervals plus a sentinel for the
interpolation). Every wrapper converts its angle to BAM and calls the
same core routine. The internal table stores values in s0.15; the
output functions shift the result to **s15.16** (radix 16), giving
exact 1.0 representation. Cardinal angles (0°, 90°, 180°, 270°)
produce exact results: `cos(0°) = 65536`, `cos(90°) = 0`,
`sin(90°) = 65536`, etc. The constant `FR_TRIG_ONE` (65536)
represents exactly 1.0 in the s15.16 output format.

### BAM-native (the core)

| Function | Signature | Output |
| --- | --- | --- |
| `fr_cos_bam` | `s32 fr_cos_bam(u16 bam)` | s15.16, range [−65536, +65536]. Exact at cardinal angles. |
| `fr_sin_bam` | `s32 fr_sin_bam(u16 bam)` | s15.16. Defined as `fr_cos_bam(bam − FR_BAM_QUADRANT)`. |

### Radian-native

| Function | Signature | Notes |
| --- | --- | --- |
| `fr_cos` | `s32 fr_cos(s32 rad, u16 radix)` | `rad` is interpreted at the given radix. Result is s15.16. |
| `fr_sin` | `s32 fr_sin(s32 rad, u16 radix)` | Same convention. |
| `fr_tan` | `s32 fr_tan(s32 rad, u16 radix)` | Returns at **radix 16** (`FR_TRIG_OUT_PREC`). Computed as `(sin << 16) / cos`; saturates to `±INT32_MAX` (`FR_TRIG_MAXVAL`) near π/2 + kπ where cos → 0. |

### Integer-degree wrappers (legacy API)

The uppercase legacy API takes an angle in degrees.
`FR_SinI`, `FR_CosI` and `FR_TanI`
take plain integer degrees — the trailing *I* denotes
*integer*. The variants *without* the `I`
suffix (`FR_Sin`, `FR_Cos`, `FR_Tan`)
accept a `radix` argument and treat the degree value as
*fixed-point*, so you can pass fractional degrees like
42.375°.

| Symbol | Signature | Kind |
| --- | --- | --- |
| `FR_SinI` | `FR_SinI(deg)` → `s32` (s15.16) | Macro: `fr_sin_bam(FR_DEG2BAM(deg))`. Zero-cost inline. |
| `FR_CosI` | `FR_CosI(deg)` → `s32` (s15.16) | Macro: `fr_cos_bam(FR_DEG2BAM(deg))`. |
| `FR_TanI` | `s32 FR_TanI(s16 deg)` | Function. Returns at radix 16; saturates to `±INT32_MAX` near 90° / 270°. |
| `FR_Sin` | `s32 FR_Sin(s16 deg, u16 radix)` | `deg` is fixed-point at `radix`. Returns s15.16. |
| `FR_Cos` | `s32 FR_Cos(s16 deg, u16 radix)` | Same. |
| `FR_Tan` | `s32 FR_Tan(s16 deg, u16 radix)` | Returns at radix 16; saturates to `±INT32_MAX` near 90° / 270°. |

### Degree wrappers on the BAM path

If you're using the lowercase family and want to skip the
radix entirely, two convenience macros cover pure integer degrees:

| Macro | Expansion |
| --- | --- |
| `fr_cos_deg(deg)` | `fr_cos_bam(FR_DEG2BAM(deg))` |
| `fr_sin_deg(deg)` | `fr_sin_bam(FR_DEG2BAM(deg))` |

## Inverse trigonometry

Every inverse-trig function in FR_Math returns the angle *in
radians* as an `s32` at a caller-specified output radix. This
makes the inverse functions symmetric with the forward trig
functions, which also work in radians. The `out_radix` parameter
lets you choose the precision of the returned angle independently
from the input.

| Function | Signature | Output range |
| --- | --- | --- |
| `FR_atan` | `s32 FR_atan(s32 input, u16 radix, u16 out_radix)` | [−π/2, +π/2] radians at `out_radix`. `input` interpreted at `radix`. |
| `FR_atan2` | `s32 FR_atan2(s32 y, s32 x, u16 out_radix)` | Full-circle [−π, +π] radians at `out_radix`. |
| `FR_asin` | `s32 FR_asin(s32 input, u16 radix, u16 out_radix)` | [−π/2, +π/2] radians at `out_radix`. Returns `FR_DOMAIN_ERROR` for |input| > 1. |
| `FR_acos` | `s32 FR_acos(s32 input, u16 radix, u16 out_radix)` | [0, +π] radians at `out_radix`. Same domain check as `FR_asin`. |

To convert the radian result to degrees, use `FR_RAD2DEG`:
`s32 deg = FR_RAD2DEG(FR_atan2(y, x, 16));`

## Logarithm and exponential

The logarithm functions are genuinely mixed-radix: they take
**three** arguments — the input value, the input
radix, and a separate output radix. That matters because logarithms
compress a huge dynamic range into a much smaller one, so the useful
fractional precision of the answer is often very different from the
radix of the input.

All three logarithms share a single implementation: find the
leading bit position of the input (that gives you the integer part
of `log2`), then interpolate a 65-entry mantissa table for
the fractional part, then scale the result to the requested output
radix. `FR_ln` and `FR_log10` multiply the
`log2` result by the appropriate radix-28 constant via
`FR_MULK28` (`FR_krLOG2E_28` or `FR_krLOG2_10_28`) before
returning.

### Logarithms

| Function | Inputs | Output | Domain / precision |
| --- | --- | --- | --- |
| `FR_log2` | `s32 input` at radix `in_r`<br>`u16 in_r` — input radix<br>`u16 out_r` — output radix | `s32` at radix `out_r`. Worst-case error ≤ 4 LSB at Q16.16. | Domain: `input > 0`. Returns `FR_DOMAIN_ERROR` for `input ≤ 0`. |
| `FR_ln` | Same shape as `FR_log2`. | `s32` at radix `out_r`. Natural log. | Same domain. Internally computed as `FR_log2(x) × ln(2)`. |
| `FR_log10` | Same shape as `FR_log2`. | `s32` at radix `out_r`. Common log. | Same domain. Internally computed as `FR_log2(x) / log2(10)`. |

**Worked example.** You have an audio-meter input
at radix 16, and you want the level in "deci-bels-ish"
units as a plain integer (no fractional bits):

```c
s32 x      = FR_NUM(2, 0, 0, 16);   /* 2.0 at radix 16 */
s32 log2_x = FR_log2(x, 16, 0);     /* output at radix 0 → plain integer */
/* log2_x == 1 */
```

If you instead wanted four fractional bits in the answer, ask
for `out_r = 4` and you'll get
`16 = 1.0 << 4`. Because input and output radix are
independent, you can freely compress a radix-16 input into a radix-4
output or expand a radix-4 input into a radix-16 output, whichever
suits your downstream math.

### Exponentials

The exponential functions use a single core routine (`FR_pow2`) plus
base-conversion macros that rescale the input. `FR_EXP` and `FR_POW10`
use `FR_MULK28` (a radix-28 constant multiply via 64-bit intermediate)
for high accuracy. Shift-only variants (`FR_EXP_FAST`, `FR_POW10_FAST`)
are available for targets where 32×32→64 multiply is expensive.

| Symbol | Kind | Inputs | Output | Notes |
| --- | --- | --- | --- | --- |
| `FR_pow2` | Function | `s32 input` at `radix` (exponent)<br>`u16 radix` | `s32` at the **same radix**. | Domain: input up to `30 << radix`; above that, saturates to `FR_OVERFLOW_POS`. Negative inputs are floored toward −∞ before splitting into integer + fractional parts. Uses a 65-entry lookup table (260 bytes) with linear interpolation. |
| `FR_EXP` | Macro | Same as `FR_pow2`. | `s32` at `radix`. | Expands to `FR_pow2(FR_MULK28(input, FR_kLOG2E_28), radix)`. Uses a radix-28 constant for ~9 digits of precision in the base conversion. |
| `FR_POW10` | Macro | Same as `FR_pow2`. | `s32` at `radix`. | Expands to `FR_pow2(FR_MULK28(input, FR_kLOG2_10_28), radix)`. Saturates around `input ≥ 9 << radix`. |
| `FR_EXP_FAST` | Macro | Same as `FR_pow2`. | `s32` at `radix`. | Shift-only variant: `FR_pow2(FR_SLOG2E(input), radix)`. No multiply instruction. Lower accuracy (~5–10 LSB at Q16.16). |
| `FR_POW10_FAST` | Macro | Same as `FR_pow2`. | `s32` at `radix`. | Shift-only variant: `FR_pow2(FR_SLOG2_10(input), radix)`. No multiply instruction. |
| `FR_MULK28` | Macro | `s32 x`, radix-28 constant `k` | `s32` at the same radix as `x`. | Multiplies a fixed-point value by a radix-28 constant using a 64-bit intermediate. Rounds to nearest. Used internally by `FR_EXP`, `FR_POW10`, `FR_ln`, `FR_log10`. |

**Worked example.** Compute `e^1.5`
at radix 16:

```c
s32 x = FR_NUM(1, 5, 1, 16);   /* 1.5 at radix 16 */
s32 y = FR_EXP(x, 16);          /* ≈ 4.4817 at radix 16 */
```

**`FR_EXP` vs `FR_EXP_FAST`.**
`FR_EXP` uses `FR_MULK28` which requires a single 32×32→64 multiply
(available on all Cortex-M, RISC-V RV32IM, x86, etc.). The radix-28
constant has ~9 decimal digits of precision, so the base conversion
adds negligible error. `FR_EXP_FAST` uses the shift-only macro
`FR_SLOG2E` which avoids all multiply instructions but introduces
~5–10 LSB of extra error at Q16.16. Use `FR_EXP_FAST` on 8-bit
targets (AVR, 8051) where 64-bit multiply is very expensive.

## Roots

| Function | Inputs | Output | Notes |
| --- | --- | --- | --- |
| `FR_sqrt` | `s32 input` at `radix`<br>`u16 radix` | `s32` at the **same radix**. | Domain: `input ≥ 0`. Returns `FR_DOMAIN_ERROR` for negative input. Digit-by-digit integer isqrt on an `int64_t` accumulator — deterministic 32-iteration cost, no floating point anywhere. **Rounds to nearest** (remainder > root → +1). Worst-case error is ±0.5 LSB at the input radix. |
| `FR_hypot` | `s32 x`, `s32 y` both at `radix`<br>`u16 radix` | `s32` at the **same radix**. | Overflow-safe magnitude: computes `sqrt(x² + y²)` without an intermediate 32-bit overflow by promoting the sum of squares to `int64_t`. Accepts the full `s32` input range; output saturates at `FR_OVERFLOW_POS` only if the true hypot exceeds `2^31−1` at the given radix. |
| `FR_hypot_fast8` | `s32 x`, `s32 y` (any radix) | `s32` at the same radix. | 8-segment shift-only piecewise-linear approximate magnitude. ~0.14% peak error. No multiply, no 64-bit, no ROM table. Based on the method of US Patent 6,567,777 B1 (public domain). No `radix` parameter needed — the algorithm is scale-invariant. |

## Wave generators

The wave generators are the same family of synth-style shapes
you'd find in a hardware LFO: square, PWM, triangle, saw,
morphing triangle, and noise. Every one of them is a pure function
with no internal state — you drive them from a phase
accumulator of your own, which lets you instance as many
independent oscillators as you like without worrying about shared
globals.

The phase is a `u16` BAM value (full circle =
65536), so advancing a phase accumulator modulo 2^16 is
free: `phase += inc;` wraps automatically because the
variable is 16-bit. Combine that with `FR_HZ2BAM_INC`
and you get a classic NCO (numerically controlled oscillator) in
two lines of code.

### The generators

| Function | Inputs | Output | Shape |
| --- | --- | --- | --- |
| `fr_wave_sqr` | `u16 phase` (BAM) | `s16` in s0.15, **bipolar** [−32767, +32767] | 50% square. Returns `+32767` for the first half of the cycle and `−32767` for the second. |
| `fr_wave_pwm` | `u16 phase` (BAM)<br>`u16 duty` — threshold; 32768 = 50% | `s16` s0.15, bipolar. | Variable-duty pulse. Returns `+32767` while `phase < duty`, else `−32767`. `duty = 0` means always low; `duty = 65535` means almost always high. |
| `fr_wave_tri` | `u16 phase` (BAM) | `s16` s0.15, bipolar. | Symmetric triangle. Peaks at `phase = 16384` (quarter cycle) and `phase = 49152`. |
| `fr_wave_saw` | `u16 phase` (BAM) | `s16` s0.15, bipolar. | Rising sawtooth. Linear ramp from `−32767` at `phase = 0` to `+32767` at `phase = 65535`, then snaps back. |
| `fr_wave_tri_morph` | `u16 phase` (BAM)<br>`u16 break_point` — BAM position of the peak | `s16` in [0, 32767] — **unipolar**. | Variable-symmetry triangle. With `break_point = 32768` you get a symmetric triangle; with `break_point` near 0 or 65535 you get a ramp-up or ramp-down saw. **Output is unipolar** — subtract 16384 and double if you need it bipolar. |
| `fr_wave_noise` | `u32 *state` — non-zero seed | `s16` s0.15, bipolar. | LFSR / xorshift pseudorandom noise. Caller owns the 32-bit state; each call advances it in place. Seed with any non-zero value. Period is `2^32 − 1`. |

### Phase increment helper

| Macro | Inputs | Output | Notes |
| --- | --- | --- | --- |
| `FR_HZ2BAM_INC(hz, sample_rate)` | Integer `hz`, integer `sample_rate` (both Hz) | `u16` phase increment | Per-sample BAM delta for a target frequency. Caller must ensure `hz < sample_rate/2` (Nyquist); the macro does not check. Both args are evaluated once. |

### Worked example — 440 Hz triangle at 48 kHz

```c
u16 phase = 0;
u16 inc   = FR_HZ2BAM_INC(440, 48000);    /* ~601 */

for (int i = 0; i < n_samples; ++i) {
    out[i] = fr_wave_tri(phase);           /* s0.15 bipolar */
    phase += inc;                           /* wraps mod 2^16 for free */
}
```

## ADSR envelope

The envelope generator is a five-state machine — Idle,
Attack, Decay, Sustain, Release — with linear segments
between states. It's sample-rate agnostic: durations are
expressed in *samples*, not seconds, so the same envelope
code runs at any rate from 1 kHz (servo loops) to 192 kHz
(audio).

Internal state lives in a caller-allocated `fr_adsr_t`
struct. No malloc, no globals; you can instance as many envelopes
as you want. The internal level is held in **s1.30**
rather than s0.15 so that very long envelopes at high sample rates
still get a non-zero per-sample increment — a 2-second attack
at 48 kHz would underflow s0.15 arithmetic in the first
sample.

### The struct

```c
typedef struct fr_adsr_s {
    u8  state;        /* FR_ADSR_IDLE|ATTACK|DECAY|SUSTAIN|RELEASE */
    s32 level;        /* current envelope value, s1.30 */
    s32 sustain;      /* sustain target,         s1.30 */
    s32 attack_inc;   /* per-sample increment during attack */
    s32 decay_dec;    /* per-sample decrement during decay */
    s32 release_dec;  /* per-sample decrement during release */
} fr_adsr_t;
```

The `FR_ADSR_*` state constants are exposed so you can
check `env.state == FR_ADSR_IDLE` to know when an
envelope has finished releasing and it's safe to recycle the
voice.

### The API

| Function | Inputs | Output | Effect |
| --- | --- | --- | --- |
| `fr_adsr_init` | `fr_adsr_t *env`<br>`u32 attack_samples`<br>`u32 decay_samples`<br>`s16 sustain_level_s015` — s0.15, range [0, 32767]<br>`u32 release_samples` | `void` | Sets the per-sample increments from the sample counts and stores the sustain target in s1.30. Envelope state becomes `FR_ADSR_IDLE`. Safe to call on an active envelope to retune it. |
| `fr_adsr_trigger` | `fr_adsr_t *env` | `void` | Note-on. Resets `level` to 0 and state to `FR_ADSR_ATTACK`. Call each time a voice should start. |
| `fr_adsr_release` | `fr_adsr_t *env` | `void` | Note-off. Jumps directly to `FR_ADSR_RELEASE` from whatever state the envelope was in — so releasing mid-attack is valid and produces a clean fade from the current level. |
| `fr_adsr_step` | `fr_adsr_t *env` | `s16` in s0.15, range [0, 32767] | Advance one sample and return the current envelope value. The returned value is **unipolar** (never negative) — multiply your bipolar oscillator by this envelope and you get an amplitude-modulated voice. |

### Worked example — 440 Hz triangle with an envelope

```c
fr_adsr_t env;
fr_adsr_init(&env,
             4800,      /* 100 ms attack  @ 48 kHz */
             9600,      /* 200 ms decay   */
             24576,     /* sustain = 0.75 in s0.15 */
             19200);    /* 400 ms release */

u16 phase = 0, inc = FR_HZ2BAM_INC(440, 48000);

fr_adsr_trigger(&env);
for (int i = 0; i < 48000; ++i) {           /* 1 second of tone */
    if (i == 24000) fr_adsr_release(&env);  /* note-off at 500 ms */
    s32 tone = fr_wave_tri(phase);              /* bipolar s0.15   */
    s32 amp  = fr_adsr_step(&env);          /* unipolar s0.15  */
    out[i]   = (s16)((tone * amp) >> 15);    /* multiplied       */
    phase += inc;
}
```

## 2D transforms (`FR_math_2D.h`)

`FR_Matrix2D_CPT` ("*C*oordinate
*P*oint *T*ransform") is a lightweight C++ class
wrapping the top two rows of a 3×3 affine matrix at a
configurable radix. The bottom row is always
`[0, 0, 1]` — that's what makes the transforms
*affine* — so it is never stored, which saves both
memory and multiplies on the transform path.

The class has no virtual methods, no dynamic allocation, and no
hidden state beyond the six stored matrix elements, a radix, and a
"fast" flag that remembers when the off-diagonal
elements are zero (pure scale / translate). That last flag lets the
transform methods take a shorter path when you haven't
rotated or sheared.

### The struct

```c
struct FR_Matrix2D_CPT {
    s32 m00, m01, m02;   /* row 0 */
    s32 m10, m11, m12;   /* row 1 */
    u16 radix;           /* radix used by the elements */
    int fast;            /* set by checkfast(): 1 if m01==0 and m10==0 */
    /* ... methods below ... */
};
```

### Construction and identity

| Method | Effect |
| --- | --- |
| `FR_Matrix2D_CPT(u16 radix = FR_MAT_DEFPREC)` | Constructor. `FR_MAT_DEFPREC = 8`, so the default is a radix-8 matrix. Calls `ID()` to load the identity. |
| `void ID()` | Load the identity matrix. |
| `void set(s32 a00, s32 a01, s32 a02, s32 a10, s32 a11, s32 a12, u16 nRadix = FR_MAT_DEFPREC)` | Set all six elements and the radix in one call. |
| `bool checkfast()` | Recompute the `fast` flag after manual edits to `m01` / `m10`. The transform methods check this flag — if you bypass the setters, call this. |

### Composition

| Method | Inputs | Effect |
| --- | --- | --- |
| `void setrotate(s16 deg)` | Integer degrees. | Overwrite the 2×2 upper-left with a rotation matrix. Uses `FR_SinI` / `FR_CosI` (BAM core). |
| `void setrotate(s16 deg, u16 deg_radix)` | Fixed-point degrees at the given radix. | Same, but lets you pass fractional degrees (e.g. 42.5° at `deg_radix = 1`). |
| `void XlateI(s32 x, s32 y)` | Integer pixel offsets. | Set the translation elements to `(x, y)`, scaling by the matrix's own radix. Overwrites whatever was in `m02` / `m12`. |
| `void XlateI(s32 x, s32 y, u16 nRadix)` | Integer offsets at an explicit radix. | As above but with a caller-supplied radix. |
| `void XlateRelativeI(s32 x, s32 y)` | Integer deltas. | *Add* to the current translation rather than overwriting it. |
| `void XlateRelativeI(s32 x, s32 y, u16 nRadix)` | Integer deltas at an explicit radix. | As above with caller-supplied radix. |
| `void add(const FR_Matrix2D_CPT *pAdd)` | Pointer to source matrix (same radix). | Element-wise `this += *pAdd`. |
| `void sub(const FR_Matrix2D_CPT *pSub)` | Pointer to source matrix (same radix). | Element-wise `this −= *pSub`. |
| `operator=`, `operator+=`, `operator−=`, `operator*=` | Reference to another matrix (or `s32` scalar for `*=`). | Idiomatic C++ convenience wrappers around the above. |

### Determinant and inverse

| Method | Inputs | Output | Notes |
| --- | --- | --- | --- |
| `s32 det()` | None | `s32` at the matrix radix | Computes the determinant of the upper-left 2×2 block (the affine translation does not contribute). Worth checking for zero before calling `inv()`. |
| `bool inv(FR_Matrix2D_CPT *nInv)` | Pointer to a *different* destination matrix | `true` on success, `false` on singular or aliased (`nInv == this`) | Out-of-place inverse; the source is preserved. |
| `bool inv()` | None | `true` on success, `false` on singular | In-place inverse. Cheaper than the out-of-place form if you don't need the original. |

### Point transforms

All point transforms take *explicit* x, y coordinates
and write to caller-supplied output pointers — there is no
packaged `Point` struct. This keeps the ABI simple and
lets you feed integer coordinates, `s16` coordinates, or
anything else without wrapping them.

| Method | Inputs | Output | Notes |
| --- | --- | --- | --- |
| `XFormPtI(s32 x, s32 y, s32 *xp, s32 *yp, u16 r)` | `s32` integer coordinates; explicit shift `r`. | Writes transformed `s32` to `*xp`, `*yp`. | Shifts the 2×2 product down by `r` at the end — pass `matrix.radix` to get integer output. |
| `XFormPtI(s32 x, s32 y, s32 *xp, s32 *yp)` | Integer `x`, `y`. | Transformed coordinates, shifted by `this->radix`. | Convenience overload that uses the matrix's stored radix. |
| `XFormPtINoTranslate(s32 x, s32 y, s32 *xp, s32 *yp, u16 r)` | Integer `x`, `y`; explicit shift. | Transformed coordinates. | Applies the 2×2 linear part only; ignores `m02` / `m12`. Useful for rotating vectors (directions) rather than points. |
| `XFormPtI16(s16 x, s16 y, s16 *xp, s16 *yp)` | `s16` integer coordinates. | `s16` output. | Fast path for sub-32-bit coordinate work. Internally promotes to `s32` during the multiply-add, then narrows back. Watch for overflow on extreme inputs. |
| `XFormPtI16NoTranslate(s16 x, s16 y, s16 *xp, s16 *yp)` | `s16` integer coordinates. | `s16` output. | Linear-only variant of the s16 path. |

The `fast` flag on the matrix causes all of these
methods to skip the cross-term multiply-adds when `m01`
and `m10` are both zero (pure scale / translate), saving
two multiplies per point.

## Formatted output

| Function | Signature |
| --- | --- |
| `FR_printNumD` | Pretty-print an `s32` as signed decimal. |
| `FR_printNumF` | Pretty-print an `s32` as fixed-point at a given radix (e.g. "3.14159"). |
| `FR_printNumH` | Pretty-print an `s32` as hex. |

All three take a user-supplied `putc` callback so you
can direct output at a UART, an in-memory buffer, or
`stdout` without pulling in `printf`.

## See also

- [Fixed-Point Primer](fixed-point-primer.md)
  — what radixes are and how to pick one.
- [Examples](examples.md) — runnable snippets
  for every topic below.
- [Getting Started](getting-started.md) —
  build and run your first FR_Math program.
