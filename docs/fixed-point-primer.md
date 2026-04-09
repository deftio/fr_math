# Fixed-Point Primer

This page walks through everything you need to use FR_Math well.
We'll start with the obvious question — *why would I
bother with fixed-point in 2026?* — and work our way up to
radix choice, overflow, and the little gotchas that catch everyone
the first time. No prior exposure to fixed-point is assumed. If you
*do* have prior exposure, the notation and worked examples
should still be useful, because FR_Math has its own conventions and
it's better to see them spelled out than to guess.

One promise up front: we're going to *show* a lot
more than we *tell*. Every abstract rule in this page is
followed by a concrete example you can type into a compiler.

---

## Why fixed-point is still a good idea

Every CPU arithmetic unit (ALU) speaks integers as its native
tongue. Add two 32-bit integers, you get a 33-bit result. Multiply
them, you get a 64-bit result. This silicon is fast, deterministic,
and has existed on every chip since the 1970s. An 8051 can add two
16-bit integers in a couple of cycles. A modern ARM or RISC-V can
do the same in a single cycle and pipeline dozens in parallel.

Fractional numbers don't exist natively in that ALU. If you
want to represent `3.14`, you have two paths:

1. **Floating point.** Either a hardware IEEE-754
   FPU or a software library that emulates one. The FPU gives
   you enormous dynamic range (roughly 10^−38 to
   10^38) at the cost of extra silicon and an error
   model most programmers never actually read. The software
   library gives you the same semantics at 10× to
   100× the speed cost of hardware float, and pulls in
   tens of kilobytes of code. On small MCUs that's more
   than your whole application.
2. **Fixed point.** You agree with yourself that
   some of your integer bits represent fractional quantities, and
   you do the bookkeeping by hand. In return: integer speed, tiny
   code, and bit-exact reproducibility across any compiler and
   architecture.

FR_Math takes option 2 — but tries to make the bookkeeping
as painless as a float would be, by giving you macros and functions
that know about the radix and do the shifts for you. You still get
to *choose* the radix per call, which is where the real
performance wins come from. More on that shortly.

---

## The core trick, with a thermometer

The easiest way to see fixed-point is to build a toy example. Say
you're writing firmware for a cheap digital thermometer. The
spec is:

- Range: −64 °C to +64 °C (good enough for a
  kitchen probe).
- Resolution: 0.5 °C.
- Storage per reading: 8 bits (you're logging a whole day
  and RAM is tight).

An 8-bit signed integer has a range of −128…+127
— 256 distinct values. And you need
(64 − (−64)) / 0.5 = **256 distinct readings**.
What a pleasant coincidence. Every reading can be stored in one
byte, as long as you agree with yourself that *the stored integer
is the real temperature times 2*. Let's write that
agreement out:

```
Real temperature     Stored byte
     25.0  °C      →  50
     25.5  °C      →  51
    -13.5  °C      → -27
     -0.5  °C      →  -1
      0.0  °C      →   0
```

Now let's see that the arithmetic still works. Add 25.0 °C
and -13.5 °C, which should give 11.5 °C:

```
   50 + (-27) = 23        ← stored integers, plain int math
25.0 + (-13.5) = 11.5     ← what they mean
```

And indeed `23 / 2 = 11.5`. The CPU never saw a
fraction; it did a perfectly ordinary 8-bit add. You just had to
*remember* to divide by 2 when you wanted to display or
reason about the value. That "divide by 2" is the scale
factor. If you pick a scale factor that's a power of two, that
division collapses into a single shift instruction, which on every
CPU ever made is either free or close to it.

Congratulations: that's fixed-point math. The rest of this
page is about conventions, arithmetic between different scales, and
how to avoid overflow — but the foundation is just "agree
on a scale, and do integer math".

---

## Notation: sM.N and the radix

We need a short way to say "this integer represents a
fractional value with N bits below the binary point". Borrowing
from the DSP world, FR_Math writes it as **sM.N**:

| Letter | Meaning |
| --- | --- |
| `s` | signed (2's complement). Use `u` instead for unsigned. |
| `M` | integer bits in use (not counting the sign bit). |
| `N` | fractional bits in use. **This is the radix.** |

So `s11.4` is a signed number with 11 bits of integer
precision and 4 bits of fractional precision. Total bits in use:
1 (sign) + 11 + 4 = 16. It happens to fit in a 16-bit register
exactly, with zero headroom. You could also drop the same bit
pattern into a 32-bit register — then you'd have
16 bits of headroom to soak up intermediate results. Same value,
same meaning; only the available headroom changes.

The scale factor for a radix-`r` number is
`2^r`, so the two conversions you'll do
constantly are:

```
real_value     = integer_value / (2^radix)
integer_value  = real_value     * (2^radix)
```

Let's try a couple on `3.5`:

- At radix 4: `3.5 × 16 = 56`, which is
  `0x38`.
- At radix 8: `3.5 × 256 = 896`, which is
  `0x380`.
- At radix 16: `3.5 × 65536 = 229376`, which is
  `0x00038000`.

Same real-world value, different bit pattern, different amounts
of fractional resolution available to downstream operations. That
trade-off — range vs. precision inside a fixed register width
— is the thing you really want to get comfortable making.
We'll come back to it in "Choosing a radix" below.

One convention worth calling out: FR_Math does not store the
radix next to the value. A fixed-point number in this library is
just an `s16` or an `s32`; the radix lives in
your head (or a `#define`) and gets passed as a parameter
when you call one of the library functions. This is a deliberate
choice, not laziness: it's what keeps the generated code tight
enough to run inside a scanline renderer or an audio inner loop. If
you want to think of an FR_Math value as a "number with a
radix", think of the radix as a *type annotation that lives
in your source code*, not a runtime field.

---

## Arithmetic: what the operations actually do

Once you've chosen a radix, the everyday operations behave
almost like integer math — with one or two twists per
operation that you just have to internalise. Let's walk
through them.

### Addition and subtraction

```
a (s11.4) + b (s11.4)  →  c (s12.4)
```

Rule: the fractional portion doesn't grow on addition,
but the integer portion grows by one bit. If you add two
`s11.4` values, the worst-case result needs an extra
integer bit — so it's `s12.4`. Add a million
of them and you need `log2(1e6) ≈ 20`
extra integer bits — now you're an `s31.4`
number and you're one addition away from overflowing a 32-bit
register.

The other rule: both operands must have the same radix before
you add them. If they don't, one of them has to move:

```c
s32 a = /* some s11.4 value */;
s32 b = /* some s9.6  value */;

/* Option 1: align b down to radix 4 (loses 2 fractional bits of b). */
s32 b_as_r4 = b >> (6 - 4);
s32 sum_r4  = a + b_as_r4;   /* still at radix 4 */

/* Option 2: align a up to radix 6 (keeps all the precision, but
 *           uses 2 more integer bits in the register).            */
s32 a_as_r6 = a << (6 - 4);
s32 sum_r6  = a_as_r6 + b;   /* now at radix 6 */
```

Neither option is wrong — the right call depends on what
the next operation in your pipeline needs. FR_Math hands you
`FR_ADD(x, xr, y, yr, r)` which does the alignment for
you and produces a result at whatever radix `r` you
request. The macro picks the shift direction; you just say what
radix you want out.

### Multiplication

```
a (sM1.N1) × b (sM2.N2)  →  c (s(M1+M2).(N1+N2))
```

Multiplying is where the fractional bits *do* grow.
Multiply two `s15.16` values and the raw product is an
`s31.32` — 64 bits wide. If you want the answer to
live in a 32-bit register, you have to shift off some of the
precision at the end. How much you drop is a design decision, and
like addition the right answer depends on your pipeline. A
concrete worked example:

```c
/* 3.5 at radix 4 is 56.  2.25 at radix 4 is 36. */
s32 a = I2FR(3, 4) | (1 << 3);   /* 3.5  */
s32 b = I2FR(2, 4) | (1 << 2);   /* 2.25 */

/* Raw 64-bit product: 56 * 36 = 2016, which has 4+4 = 8 fractional bits.
 * 2016 / 256 = 7.875 — the right answer, at radix 8.               */
s64 raw = (s64)a * (s64)b;

/* If we want the answer back at radix 4: shift off 4 fractional bits. */
s32 c = (s32)(raw >> 4);           /* 7.875 at radix 4 = 126 = 0x7E */
```

You almost never want to write those shifts by hand. FR_Math
gives you two spellings of the same idea:

- `FR_FixMuls(a, b, r)` — multiply two
  same-radix-`r` values and return the product at
  radix `r`. Uses an `int64_t`
  intermediate so the 32-bit intermediate overflow trap
  doesn't fire. **Truncates** on the way
  down — no clamp, no rounding.
- `FR_FixMulSat(a, b, r)` — same shape, but
  saturates to `FR_OVERFLOW_POS` /
  `FR_OVERFLOW_NEG` if the result wouldn't
  fit. Prefer this one by default unless you've proven
  the product stays in range.

There's also a macro form `FR_MUL(a, ar, b, br, r)`
that handles the mixed-radix case — you can multiply an
`s11.4` by an `s9.6` and ask for the answer at
radix 8 without doing any manual shifts.

### Changing radix

Sooner or later you'll have a value at one radix and need
it at another — maybe because the next function in the
pipeline wants a different convention, or because you're
committing the answer to a log buffer that uses `s7.8`.
`FR_CHRDX(value, from_r, to_r)` does exactly that shift
for you:

- Going to a *larger* radix — the new low bits are
  zeroed in. No precision is lost, but the integer headroom
  shrinks.
- Going to a *smaller* radix — the low bits are
  dropped. Precision is lost; headroom grows. This is a good
  place to add `± (1 << (from_r - to_r - 1))`
  before the shift if you want round-to-nearest behaviour.

The value is conserved as closely as the destination radix can
represent it. Nothing more, nothing less.

---

## Overflow, saturation, and the sentinels

The single biggest trap in any fixed-point code is *silent
integer overflow*. If you multiply two `s15.16` values
and store the result back into a 32-bit register without thinking
about it, you will eventually pass a pair of inputs whose product
doesn't fit, and plain C will hand you wrap-around garbage
with no warning. A signed 32-bit multiply that overflows is not a
runtime error in C — it's undefined behaviour that
happens to look like data most of the time.

FR_Math defends against this in three layers, and it's
worth knowing all three so you can decide which level of paranoia
to engage for a given piece of code.

1. **Int64 intermediates everywhere it matters.**
   Every multiply macro that could plausibly overflow a 32-bit
   intermediate promotes to `int64_t` first, does the
   multiply there, shifts, and then lands in the destination.
   This is free on any 32-bit or larger CPU and cheap on 16-bit
   chips — even sdcc on an 8051 handles it, slowly but
   correctly.
2. **Saturating variants for the common operations.**
   `FR_FixMulSat` and `FR_FixAddSat` clamp
   the result to `FR_OVERFLOW_POS` (aliases
   `INT32_MAX`) or `FR_OVERFLOW_NEG`
   (aliases `INT32_MIN + 1`) instead of wrapping.
   Think of saturation as the DSP convention: if the answer
   won't fit, return the nearest value that will. Better to
   lose precision at the extremes than to wrap a big positive
   number into a big negative one.
3. **Sentinel returns from functions that can fail.**
   A handful of functions have inputs that are simply not in
   their domain: `FR_sqrt(-1)`, `FR_log2(0)`,
   `FR_asin(x)` with `|x| > 1`, and so
   on. These return `FR_DOMAIN_ERROR`
   (`INT32_MIN`), which is a value that can never
   occur as a valid result because it sits one step below the
   saturation range. You check for it with a plain
   `if`.

In practice: call the saturating variants by default, check for
`FR_DOMAIN_ERROR` after any function whose input you
don't fully control, and in release builds on well-bounded
inputs the defensive paths never trigger. They're there for
the one time you feed `-0.0000001` into a square root at
3 AM.

---

## Choosing a radix

This is the decision most beginners rush and most experienced
fixed-point programmers spend most of their design time on. Picking
the right radix for a variable is what separates a clean pipeline
from one that's constantly overflowing or quantising away
signal. Three inputs drive the decision:

1. **Maximum absolute value** the variable will ever
   need to hold. Take `ceil(log2(max))`
   — that's how many integer bits you need.
2. **Required precision** — the smallest step
   that matters to your application. Take
   `ceil(−log2(step))` —
   that's how many fractional bits you need.
3. **Register width** available on your target CPU,
   and how much **headroom** you want inside that
   register for intermediate products.

Let's do one. Suppose you're writing a PID loop for a
motor controller. The error signal has a worst case of
±512 ticks, and you need to resolve ±0.01 ticks or the
loop will limit-cycle.

- Integer bits: `ceil(log2(512)) = 9`.
- Fractional bits: `ceil(−log2(0.01)) = 7`.
- Sign bit: 1.

Total: 9 + 7 + 1 = 17 bits in use. That doesn't fit in a
16-bit register, so you need an `s32`. You now have a
choice:

- `s9.7` — 17 bits used, 15 bits of headroom.
  That's a lot of room for intermediate products before
  saturation kicks in.
- `s9.22` — the integer bits stay at 9 but you
  promote every fractional bit you can spare. Same range, way
  more precision, but no headroom at all for an unwary
  multiply.
- Something in between, chosen by how much the intermediate
  products grow.

FR_Math encourages the "use just enough" mindset
over "always pick `s15.16` out of habit". A
narrower radix leaves more headroom, packs tighter in RAM, and
shifts faster on small cores. If you find yourself wanting
`s15.16` on every single variable, stop and ask whether
you actually need 15 integer bits on that particular signal.

---

## FR_Math's naming conventions

A tour of the casing, because it tells you something about the
generation of each symbol:

| Prefix | What it is | Example |
| --- | --- | --- |
| `FR_XXX()` | `UPPERCASE` macro — inline, zero call overhead. | `FR_ADD`, `FR_MUL`, `FR_FR2I` |
| `FR_Xxx()` | Mixed-case C function — the classic v1 API. Integer-degree trig and related. | `FR_Sin`, `FR_log2`, `FR_sqrt` |
| `fr_xxx()` | Lowercase C function — v2 additions (radian / BAM trig, wave generators, ADSR). | `fr_sin`, `fr_wave_tri`, `fr_adsr_step` |
| `s8, s16, s32` | Signed integer typedefs (aliases for `int8_t`, `int16_t`, `int32_t`). | — |
| `u8, u16, u32` | Unsigned integer typedefs. | — |

The `I2FR` / `FR2I` macros are the bridge
between plain integers and fixed-point bit patterns. They're
one-liners, but you'll type them constantly so it's
worth knowing what they actually do:

- `I2FR(i, r)` expands to `(i) << (r)`
  — take an integer, shift it up by `r` bits,
  and now it's a radix-`r` fixed-point value.
  `I2FR(3, 4)` is 48, i.e. 3.0 at s.4.
- `FR_FR2I(x, r)` expands to
  `(x) >> (r)` — the reverse. Careful:
  C's right shift on signed values truncates toward
  −∞, so `FR_FR2I(-1, 4)` is
  `-1`, not `0`. If you want
  round-to-nearest (which is usually what you want for display),
  use `FR_FR2Iround(x, r)`.

---

## Angle representations

Angles deserve their own section because FR_Math gives you
*three* ways to represent one, and each has a specific job:

1. **Integer degrees** — plain `s16`,
   usually in `[0, 360)` but negative values wrap
   correctly. Used by the classic `FR_SinI` /
   `FR_CosI` API (the *I* denotes integer).
   Friendliest if you're coming from a calculator or a
   Protractor.
2. **Radians at a programmer-chosen radix** —
   an `s32` fixed-point value. Used by the lowercase
   `fr_sin` / `fr_cos` /
   `fr_tan` family. Use this one when your upstream
   math is already in radians or when you want fractional angles
   with more than 360 steps of resolution.
3. **BAM** (Binary Angular Measure) — a
   `u16` where the full circle maps to
   `[0, 65536)`. BAM is the native representation of
   the internal trig table: the top 2 bits select the quadrant,
   the next 7 bits index the table, and the bottom 7 bits drive
   linear interpolation. The big reason to care is that BAM
   *wraps for free* — every time you add to a
   `u16` phase accumulator, the overflow is exactly
   2π. No modulo, no branch, no glitch at the boundary. This
   is what makes BAM the right choice for audio oscillators,
   LFOs, and scanning phase accumulators.

**Why is BAM a `u16` and not an `s32` you could pass any signed
angle into?** Because the `u16` wraparound *is* the angular
modulus — that's the whole feature. Adding two `u16` BAM values
automatically gives you the right answer modulo a full revolution,
with zero quantisation error at the boundary and no `% 65536` in
sight. If BAM were `s32`, every read of the table would have to
explicitly mask off the top bits (and handle negative values)
before the quadrant extraction (`bam >> 14`) made any sense. You
would have traded one free operation for two slow ones on every
sample, just to get back the same behaviour. So instead, the public
trig entry points (`FR_CosI`, `FR_Cos`, `fr_cos`, and friends)
*all* take signed angles — in degrees, fixed-radix degrees, or
radians — and only the internal `fr_cos_bam` / `fr_sin_bam`
primitives see the `u16`. In practice you will never construct a
BAM by hand unless you are specifically building a phase
accumulator, in which case the wraparound is exactly what you want
anyway.

FR_Math gives you the conversion macros `FR_DEG2BAM`,
`FR_BAM2DEG`, `FR_RAD2BAM`, and
`FR_BAM2RAD`, all chosen so that the compiler can drop
back to shift-and-add on chips without a hardware multiplier. The
[API reference](api-reference.md) walks through the
constants and has a worked example showing how `FR_BAM2DEG`
turns into four shifts and three adds, if you're curious why
the numbers look the way they do.

---

## Going deeper

That's the whole mental model. When you hit something in
the code that feels surprising, come back to this page — the
answer is almost always "track the radix more carefully"
or "watch for the intermediate product". For everything
else:

- **[API Reference](api-reference.md)**
  — every public symbol with its radix, inputs, outputs,
  error sentinels, and worst-case error.
- **[Examples](examples.md)** —
  runnable snippets for every module, including a phase
  accumulator, a 2D rotation, and a PID loop.
- **[Getting Started](getting-started.md)**
  — if you haven't built the library yet, start
  here.

If something here is wrong or unclear, the fastest fix is a
GitHub issue or a PR on the `pages/` directory. This
primer is part of the repo, so it can and should get better as
people run into edge cases worth writing down.
