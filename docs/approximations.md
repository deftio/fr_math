# How the Approximations Work

The [Fixed-Point Primer](fixed-point-primer.md) explains how to store
and combine fractional numbers in integers. This page covers the next
question: once you have fixed-point numbers, how do you compute
`sin`, `log2`, `exp` or `sqrt` of one with nothing but integer adds,
shifts, the occasional multiply, and a few hundred bytes of tables?

Every function family in FR_Math is built from the same small toolbox.
We'll go through the toolbox first, then walk through each family:
what it does, why it was built that way, where its error comes from,
and how big that error is. The numbers on this page are measured, not
estimated. The test suite checks every function on every input at
every radix from 0 to 16 (see [How we measure](#how-we-measure)).

## The constraints

A math library for a PC can assume a floating-point unit, a fast
hardware divider and megabytes of memory. FR_Math assumes none of
them. The design rules are:

- **Integers only.** No `float` or `double` anywhere in the library,
  so it runs on chips without an FPU and gives bit-identical results
  on every compiler.
- **Divides are expensive.** Many small cores have no hardware
  divider: a 32-bit divide is a library call costing tens to hundreds
  of cycles. We use divides only where nothing cheaper works, and
  never a 64-bit divide on a hot path.
- **Multiplies may be expensive too.** An 8051 or an AVR multiplies
  slowly, or only 8 bits at a time. Wherever a multiply is by a fixed
  constant, FR_Math can replace it with a few shifts and adds.
- **Tables must be small.** The four main tables together are under
  1 KB of ROM: 258 bytes for sine, 130 for tangent, and 260 each for
  `log2` and `2^x`.
- **The caller picks the radix.** Every function takes the binary
  point as an argument, so the algorithm has to work at radix 0, 16
  or 30 alike.

Under those rules, the textbook approach of a Taylor series is usually
the wrong tool. A series needs many multiplies, and its error is
smallest near one point and grows away from it. The tools below give a
bounded error over the whole input range for a handful of cycles.

## The toolbox

### 1. Range reduction: shrink the problem

Most functions have symmetries that let you compute a small piece of
the domain and derive the rest exactly:

- `sin` and `cos` repeat every 2π, and every quadrant is a mirror or a
  negation of the first one. So we only need sin on [0, π/2].
- `tan` repeats every π, and `tan(x) = 1 / tan(π/2 − x)`. So we only
  need tan on [0, π/4].
- `log2(x)` for any positive `x` is an integer (the position of the
  top bit) plus `log2` of a mantissa in [1, 2).
- `2^x` is a shift by the integer part of `x` times `2^f` for a
  fraction `f` in [0, 1).

Range reduction is exact, or close to it: it moves bits around
without rounding them. The approximation only has to be good on the
small reduced range, and that is where a table works well.

### 2. A small table plus linear interpolation

Store the function at evenly spaced points, find the two entries that
bracket the input, and draw a straight line between them:

```c
idx  = x >> FRAC_BITS;            /* which interval           */
frac = x &  FRAC_MASK;            /* how far into it          */
lo   = table[idx];
hi   = table[idx + 1];
y    = lo + (((hi - lo) * frac) >> FRAC_BITS);
```

That is one multiply, two table reads, and no divide. When the table
size is a power of two, the index and fraction are just the top and
bottom bits of the input.

The error has a simple bound. For a function `f` sampled every `h`, the
straight line between samples is off by at most

    error <= h^2 / 8 * max|f''|

where `f''` is the second derivative (the curvature). Two things follow:

- **Doubling the table quarters the error.** That is why 65 or 129
  entries is enough. Each extra doubling buys two bits of accuracy for
  twice the ROM.
- **Curvature decides the table size.** Where a function is nearly
  straight, a few entries do. Where it bends hard (tan near 90°), no
  reasonable table works, and you need a different tool (see
  [tan](#tangent)).

For the sine table (129 entries over a quarter circle,
`h = (π/2)/128`), the bound is `h²/8 ≈ 1.9e-5`. That is 0.6 LSB in the
table's s0.15 format, so interpolation adds less than one bit of error.

### 3. Shift-and-add constants

Multiplying by a fixed constant can be written as a sum of powers of
two. For example, converting radians to BAM (see
[Angles](#angles-bam-and-reduction)) multiplies by
`65536 / 2π = 10430.378350...`, which is

    2^13 + 2^11 + 2^7 + 2^6 - 2^1 + 2^-1 - 2^-3 + 2^-8 - 2^-11 - 2^-14
    = 10430.378357

so the multiply becomes ten shifts and adds, accurate to about 21 bits.
On a chip with a single-cycle multiplier this buys little, but on a
chip without one it is several times faster. `FR_EXP_FAST` and
`FR_POW10_FAST` use the same idea for `log2(e)` and `log2(10)`.

The order of the terms matters in fixed point: the partial sums must
not overflow `s32` partway through. That is why the radian-to-BAM code
mixes positive and negative terms early.

### 4. Digit-by-digit algorithms

Some functions have an exact integer algorithm that produces one bit
of the result per step, using only shifts, adds and compares. Square
root is the classic case. It is exact (round to nearest), needs no
table and no divide, and runs in a fixed number of steps, so the time
is predictable.

### 5. Special cases where the table is weakest

A table with linear interpolation has a constant absolute error. That
is fine where the function is large, but where it is near zero the
same absolute error is a large relative error, and where it is near
infinity the error grows without bound. FR_Math switches to a short
series in those regions:

| Region | Identity used | Error of the identity |
| --- | --- | --- |
| sin near 0, π, 2π | `sin(δ) ≈ δ` | `δ³/6`, under 1 LSB in the window |
| cos near π/2, 3π/2 | `cos(π/2 + δ) ≈ −δ` | same |
| tan within 10° of a pole | `cot(ε) ≈ 1/ε − ε/3` | `ε⁴/45`, ≈ 2e-5 relative at 10° |
| acos near ±1 | `acos(x) ≈ sqrt(2(1 − x))` | ≈ `(1 − x)/12` relative, under 0.03% |
| atan2 near the axes | `asin(s) ≈ s` | `s³/6` |

### 6. Piecewise-linear fits with shift-only coefficients

When a function of two inputs is needed fast and approximately, such
as the length of a vector, FR_Math fits it with a few straight-line
segments whose coefficients are sums of powers of two. That gives no
multiplies at all and a bounded, documented error. See
[`FR_hypot_fast8`](#square-root-and-hypot).

## Angles: BAM and reduction

All the forward trig functions (sin, cos, tan) share one internal
representation: **BAM**, or binary angular measure. A full circle is
65536 units, so an angle fits in a `u16`:

    bits 15-14   quadrant (0..3)
    bits 13-7    table index within the quadrant (0..127)
    bits 6-0     interpolation fraction

The quadrant, index and fraction are just bit fields, so no divide or
compare is needed to find them. And because a `u16` wraps at 65536,
adding to a BAM phase wraps at exactly 2π for free. That is why audio
oscillators and phase accumulators use BAM directly.

Callers rarely start in BAM, though. Degrees and radians must first be
reduced to one turn and converted. Degrees are easy: 360 is an integer
at any radix, so `deg % (360 << radix)` is exact. Radians are harder,
because 2π is irrational and has no exact fixed-point value. At radix
16, `2π = 411774.83` stored as the integer 411775 is off by 0.17 LSB.
Subtracting it once is harmless, but a large angle that wraps
thousands of times picks up that error every time it wraps.

FR_Math reduces large radian inputs by multiplying by 1/(2π) instead,
held to 64 bits:

    turns = (|x| * (2^64 / 2π)) mod 2^64      /* fraction of a turn */

The multiply wraps modulo 2^64 for free, which throws away the whole
turns and keeps the fractional part to 2^-32 of a turn. That gives an
exact reduction for any `s32` input at any radix, with two multiplies
and no divide. Small angles (|x| ≤ 4π) skip it and take the shorter
path.

**Where the error comes from.** Converting to a 16-bit BAM rounds the
angle to the nearest 1/65536 of a turn, which is up to
`2π/65536/2 ≈ 4.8e-5` rad. At s15.16 output that is about 3 LSB, the
largest single term in the sin/cos error budget below.

## Sine and cosine

**What it does.** `fr_sin_bam` reduces to the first quadrant by the
mirror rules (quadrants 1 and 3 read the table backwards, quadrants 2
and 3 negate), then looks up the 129-entry sine table with 7-bit
linear interpolation. Cosine is sine shifted by a quarter turn:
`cos(x) = sin(x + 16384)` in BAM, so it costs one add. The radian and
degree versions reduce the angle, convert to BAM and call the same
code.

**Special cases.** Exact multiples of 90° return exact values (0 or
±1.0). Below one table step (about 0.7°) the code returns `sin(θ) ≈ θ`
directly, because near zero that is more accurate than interpolating.
The radian functions also return the input offset directly within
256 LSB of each zero crossing, so `sin(π)` really is close to zero.

**Why a sine table and not cosine?** A sine quadrant is ascending from
0 to 1, so the same table also serves `asin` and `acos` by searching
it (see [Inverse trig](#inverse-trig)). One table, five functions.

**Error budget at radix 16.** The pieces add up:

| Source | Worst case |
| --- | --- |
| Table entries rounded to s0.15 | 0.5 LSB of s0.15 = 1 LSB at s15.16 |
| Linear interpolation, `h²/8` | ≈ 1.2 LSB at s15.16 |
| Input rounded to 16-bit BAM (rad, deg only) | ≈ 3.1 LSB at s15.16 |
| **Measured, every `s32` input** | **5.5 LSB (sin), 5.9 LSB (cos)** |

In the published table these show as 0.15% (BAM input) and 0.61%
(radian or degree input). The percentage looks large only near the
zero crossings, where the true value is tiny; see
[How we measure](#how-we-measure) for why.

## Tangent

Tangent is the hard one. It runs from 0 to infinity over a quarter
turn, and its curvature grows without bound near 90°, so no small
table can cover it. FR_Math splits the quarter turn into three
regions:

1. **0° to 45°: table.** A 65-entry table of tan over the first
   octant, with linear interpolation. Here tan stays between 0 and 1,
   and the table is accurate to about 7.5e-5 (`h²/8 × max|tan''|`). Near
   zero, the code redoes the interpolation with 4 extra bits so small
   results keep their relative accuracy.
2. **45° to 80°: reciprocal.** `tan(x) = 1 / tan(90° − x)`, and
   `90° − x` falls back in the first octant. That is one table lookup
   and one 32-bit divide.
3. **Within 10° of a pole: series.** The reciprocal amplifies any
   error in the small denominator by about `1/ε`, where `ε` is the
   distance to the pole. With a BAM input, the 0.5-unit rounding alone
   becomes a large relative error at `ε ≈ 0.01°`. So inside this
   window the code computes `cot(ε) = 1/ε − ε/3` from the *exact*
   distance to the pole, measured in 2^-32 turns rather than BAM. The
   dropped terms are under `ε⁴/45`, about 2e-5 relative at 10°. `1/ε` is
   built from two 32-bit divides; no 64-bit divide is needed.

At the pole itself, the result saturates to `±FR_TRIG_MAXVAL`
(`0x7fffffff`), with the sign that matches the side of the pole.

**Measured over every input at every radix 0 to 16:** 0.048% worst case
for radian input, 0.041% for degrees and 0.015% for BAM. All three
are worst near the poles, where the output is enormous and the
input's own rounding limits what any algorithm could do.

## Inverse trig

**`acos` and `asin`** search the sine table instead of storing a
second one. A binary search finds the two entries that bracket the
input, then one divide interpolates between them to get the angle.
That costs about eight compares and a divide, but no extra ROM. Then
`asin(x) = π/2 − acos(x)`.

The search breaks down near `x = ±1`, where the sine table is nearly
flat: many angles share almost the same value, so a tiny change in
`x` moves the angle a lot. Above `x ≈ 0.996` the code switches to
`acos(x) ≈ sqrt(2(1 − x))`, which is accurate to about `(1 − x)/12`
relative, under 0.03% in that window.

**`atan2(y, x)`** works on magnitudes and restores the quadrant from
the signs at the end:

1. Scale `|x|` and `|y|` by the same power of two so the larger one
   sits in [2^14, 2^16). That doesn't change the angle, and it gives the
   next steps enough bits even when the inputs are tiny.
2. Compute the length `h = hypot_fast8(|x|, |y|)` with the shift-only
   approximation described below.
3. Below 45°, `angle = asin(|y|/h)`; above 45°, `angle = acos(|x|/h)`.
   Choosing by octant keeps each inverse function in the region where
   it is well conditioned: a small error in the ratio never moves the
   angle by more than about 1.4 times as much.
4. Very small ratios (under about 4.8°) skip the table and use
   `asin(s) ≈ s` directly.

`atan(x)` is `atan2(x, 1)`.

This design reuses the sine table and `hypot_fast8`, so atan2 adds
almost no ROM. Its error is dominated by `hypot_fast8`'s 0.14%, which
passes straight into the ratio: 0.26% worst case. The cost is speed,
because a search and two divides make it the slowest trig function.

## Logarithms

**`log2(x)`** splits `x` into exponent and mantissa, like a float:

1. Find `p`, the position of the top set bit. Then `x = 2^p × m` with
   the mantissa `m` in [1, 2), and `log2(x) = p + log2(m)`.
2. Shift `x` so its top bit sits at bit 30. The next 6 bits index a
   65-entry table of `log2(1 + i/64)`, and the 24 bits below them
   interpolate.
3. Subtract the input radix from `p` and add the table result.

The integer part is exact, so all the error is in the mantissa table:
`h²/8 × max|f''| = (1/64)²/8 × 1/ln 2 ≈ 4.4e-5`, in units of log2.

**`ln(x)` and `log10(x)`** are `log2(x)` times a constant
(`1/log2(e)` and `1/log2(10)`), applied with one 64-bit multiply at
28 fractional bits. The constant adds no visible error; the table does.

`x <= 0` returns the sentinel `FR_LOG2MIN` instead of negative infinity.

## Powers and exponentials

**`pow2(x)`** is the mirror image of `log2`:

1. Split `x` into `floor(x)` and a fraction `f` in [0, 1). For negative
   inputs this is the mathematical floor, so −2.3 becomes −3 + 0.7.
2. Look up `2^f` in a 65-entry table with linear interpolation. The
   result is a mantissa in [1, 2).
3. Shift the mantissa left or right by `floor(x)`, straight to the
   caller's radix.

The table error is `(1/64)²/8 × max|(2^f)''| ≈ 2.9e-5` relative, and
the shift adds none. If the result can't fit in `s32`, `pow2` returns
`FR_OVERFLOW_POS`, and it underflows to 0.

**`exp(x)` and `pow10(x)`** turn into `pow2` with one multiply:

    e^x  = 2^(x × log2 e)
    10^x = 2^(x × log2 10)

`FR_EXP` and `FR_POW10` do that multiply exactly at 28 fractional bits
(one 64-bit multiply). `FR_EXP_FAST` and `FR_POW10_FAST` use a
shift-and-add version of the constant instead, accurate to about 5e-6.
That is faster on chips without a multiplier, at the cost of about
four times the error (0.010% vs 0.0025% at radix 16).

**A limit worth knowing.** The exponent `x × log2 e` is rounded to the
*input* radix before it reaches `pow2`, and an error in an exponent
becomes a relative error in the result. At radix 16, the rounding is
1/65536 and adds at most about 0.0005% to the result. At radix 0, the
exponent is rounded to a whole number, so `FR_EXP(17, 0)` computes
`2^25` instead of `2^24.53`: 39% off. The error doubles for each
radix bit you remove. If you need `exp` of an integer, pass it at a
higher radix (say 16) and convert the result.

## Square root and hypot

**`sqrt(x)`** uses the digit-by-digit method, the binary version of
long-hand square root. Each step tries setting the next bit of the
root and keeps it if the square still fits under the input. That
takes one compare, one subtract and two shifts per result bit, with no
table and no divide. A final check rounds to nearest, so the result is
always within 0.5 LSB.

To handle the radix, note that `sqrt(x / 2^r) × 2^r = sqrt(x × 2^r)`.
So the code computes the integer square root of `x << r` in a 64-bit
accumulator. That one rule makes it exact at every radix from 0 to 30.

**`hypot(x, y)`** is the same algorithm on `x² + y²`, also in 64 bits.
The sum of two squared `s32` values fits, so it never overflows, and
the result is within 0.5 LSB.

**`hypot_fast8(x, y)`** is for when you need a vector length fast and
can accept 0.14% error. It is based on the observation that
`sqrt(hi² + lo²)` depends only on the ratio `β = lo/hi`, and on each
of 8 slices of `β` the length is close to a straight line
`a × hi + b × lo`. The code finds the slice with shift-only
comparisons, then evaluates the line with coefficients that are sums
of powers of two. It uses no multiply and no divide. The method is
from US patent 6,567,777 B1 (Chatterjee), which has expired.

## Waveforms and envelopes

The wave generators (`fr_wave_sqr`, `tri`, `saw`, `pwm`, `tri_morph`)
take a BAM phase, so a `u16` accumulator plus an increment from
`FR_HZ2BAM_INC` is a complete oscillator. Each shape is a few compares
and shifts of the phase, not a table. `fr_wave_noise` is a 32-bit
Galois LFSR (one shift and a conditional XOR per sample). The ADSR
envelope is a small state machine that steps a level linearly between
targets, so it needs no exponentials.

## How we measure

Every error figure in the docs comes from a sweep over **every input**,
not a sample. Sampling misses the narrow places where approximations
fail: zero crossings, poles, domain edges, the ends of the `s32` range.
Earlier releases sampled, and real bugs hid in the gaps.

- `tests/test_exhaustive.c` (part of `make test`) runs every function
  at every radix 0..16 on every input in its domain: all 65536 BAM
  angles, every raw value in the `s16` range and in one to two full
  periods, both ends of `s32`, and a stride across the rest. Each
  function has an error budget, and the test fails if any input
  exceeds it.
- `make test-exhaustive-s32` adds every one of the 2^32 inputs at
  radix 16 for the six angle functions (about 10 minutes).
- The reference is IEEE double precision, with care where double
  itself is not enough. For example, huge radian angles are reduced
  with a two-part 2π, because `fmod(x, 2π)` in double is off by about
  1e-7 rad at 2^31.

**About the percentages.** The published table reports relative error,
`|got − exact| / |exact|`, but with `|exact|` clamped to at least 1% of
full scale. Without the clamp, any output near zero would show an
enormous relative error even when it is off by a single LSB. With it,
near zero the figure reads as absolute error divided by 0.01. That is
why sin/cos show 0.61%: about 4 LSB at s15.16 (6e-5), at an angle
where the true value is under 0.01.

The per-function figures are in the
[accuracy table](../README.md#measured-accuracy) and the
[API reference](api-reference.md).

## Choosing between variants

| You want | Use | Why |
| --- | --- | --- |
| An oscillator or phase accumulator | `fr_sin_bam`, `fr_cos_bam` with a `u16` phase | Wraps at 2π for free and skips the angle conversion. |
| sin/cos of a radian value | `fr_sin`, `fr_cos` | Any `s32` input, exact reduction. |
| tan near 90° | Any tan variant | All four handle the poles the same way. |
| An angle from a vector | `FR_atan2` | Handles all quadrants, the axes and `INT32_MIN`. |
| An exact length | `FR_hypot` | Within 0.5 LSB; one 64-bit square root. |
| A fast length, ~0.1% is fine | `FR_hypot_fast8` | No multiply, no divide. |
| `e^x` on a chip with a multiplier | `FR_EXP` | Exact constant, 0.0025% at radix 16. |
| `e^x` on a chip without one | `FR_EXP_FAST` | Shift-only constant, 0.010% at radix 16. |
| `exp` or `pow10` of small integers | Pass the input at radix 16 | The exponent is rounded at the input radix. |

## Going deeper

- [API Reference](api-reference.md): every function's inputs, outputs,
  sentinels and worst-case error.
- [Fixed-Point Primer](fixed-point-primer.md): the number format
  underneath all of this.
- The source: `src/FR_math.c` is one file, and each function's comment
  explains its algorithm and error terms in more detail.
- `tools/coef-gen.py` regenerates the tables, and
  `tools/interp_analysis.html` plots the interpolation error.
