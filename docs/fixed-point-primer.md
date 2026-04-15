# Fixed-Point Primer

This page walks through everything you need to use FR_Math well.
We'll start with the obvious question — *why would I
bother with fixed-point in 2026?* — and work our way up to
radix choice, overflow, and the little gotchas that catch everyone
the first time. No prior exposure to fixed-point is assumed. If you
*do* have prior exposure, the notation and worked examples
should still be useful, because FR_Math has its own conventions and
it's better to see them spelled out than to guess.

## When fixed-point is useful

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

## A first try: scaling in base 10

Before we jump into binary, let's do the obvious thing a programmer
would try if you handed them this problem fresh: pick a scale
factor, multiply every fractional value by it, and store the result
in a plain `int`. It's the cleanest way to see what problems
fixed-point math has to solve, before we've committed to any
particular solution.

Say we want to add `10.25` and `0.55`. Pick a scale factor of 100
so the first two decimal digits survive the round-trip through an
integer:

```
 10.25 × 100  →  1025
  0.55 × 100  →    55
 1025  +   55 →  1080
```

Divide the result by 100 at the end and you get `10.80`. That's
the right answer. Now write it in C:

```c
int a = 1025;   /* 10.25 times 100 */
int b = 55;     /* 0.55  times 100 */
int c = a + b;  /* 1080 */

printf("%d + %d = %d\n", a, b, c);
/* prints: 1025 + 55 = 1080  (not 10.25 + 0.55 = 10.80) */
```

And immediately we run into the first problem. The compiler has no
idea these numbers were meant to be scaled — to it they are plain
`int` variables holding 1025, 55, and 1080. Every time we want to
display one, compare two of them against a human-readable
threshold, or reason about the result, *we* have to do the "divide
by 100" in our head. No type system helps us. No implicit
conversion catches the mistake. We picked the scale factor, and we
own the bookkeeping forever.

That's hazard number one: **the programmer tracks the scale by
hand**. Get it wrong on one variable in one function and the bug
is silent.

## The other hazard: overflow

Hazard number two is quieter, and much more dangerous. Take two
signed 8-bit integers:

```c
signed char a = 34, b = 3, c;
c = a * b;      /* c = 102. Fine. */
```

34 × 3 is 102, which still fits in a signed byte (max `+127`). Now
change `b` to 5:

```c
b = 5;
c = a * b;      /* real answer: 170. c actually becomes -86. */
```

170 doesn't fit in a signed byte. C doesn't raise an error — it
silently wraps. 170 becomes −86, the program keeps running, and
two modules downstream a thermostat decides it's freezing. This
is called **overflow** (or wrap-around), and it is the second
thing any fixed-point code has to worry about constantly. Worse,
scaled-integer code makes it more likely on purpose: the whole
point of scaling is to cram more magnitude into a small register,
so the inputs that used to fit comfortably can now tip over the
top.

FR_Math gives you three tools for this — wider intermediates,
saturating variants, and explicit sentinel returns — and they all
get their own section below under "Overflow, saturation, and the
sentinels". For now just file it away: *scaled-integer math means
you think about overflow on every multiply*.

## Why we move to powers of two

The base-10 version above works as arithmetic, but it's not a
great use of bits. Three problems pile on top of each other:

- **Divide-by-10 is slow.** On most chips a divide is many cycles
  and can't be pipelined cheaply. Divide-by-100 is worse. That
  scale factor you picked now costs you runtime on every
  conversion — and in a real pipeline you convert a *lot*.
- **The bit count doesn't match the math.** You get 100 distinct
  fractional steps per integer unit, which isn't a power of two,
  so there's no clean way to say "this variable has *N* bits of
  fractional precision". You can only say "divided by 100", and
  you can't cleanly reason about how much integer headroom is
  left in the register.
- **Changing precision is painful.** Want to move from "scaled by
  100" to "scaled by 1000"? That's a multiply by 10 on every
  value, with all the same divide costs on the way back out
  later.

Swap the scale factor for a power of two and all three problems
evaporate. Instead of "multiplied by 100" we say "shifted left by
*N*". Now:

- The conversion is a single shift instruction, which has been
  free or close to free on every CPU ever made.
- The precision is exactly *N* bits — the same currency the
  silicon already deals in. You can see at a glance how much
  integer headroom is left.
- Moving between radixes (say, from 4 fractional bits to 8) is
  another single shift, in whichever direction you want.

This is the real reason fixed-point in the wild is almost always
binary. Base-10 scaling survives in a few corners — COBOL, some
currency code, the occasional receipt printer — but every DSP,
every embedded graphics library, and every retro console's inner
loop uses powers of two. FR_Math follows the same rule without
apology. From here on out, "scale factor" means "shift count" and
every fractional value is a signed or unsigned integer with an
agreed-upon number of low bits dedicated to the fractional part.

## The core trick, with a thermometer

The easiest way to see binary fixed-point in action is to build a
toy example small enough that the shifts are obvious. Say you're
writing firmware for a cheap digital thermometer. The spec is:

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

## Quantisation and loss of precision

Fixing the radix also fixes the smallest representable fractional
step. At radix *N*, that step is `2^−N` — nothing finer survives
the round-trip into the integer. Any real value smaller than the
step rounds to zero; any real value landing between two adjacent
steps rounds to one of them. The difference between the ideal
value and its stored form is called **quantisation error**, and it
is the main price paid for doing fractional math in integer
registers.

A concrete example. Store `0.1` as an s*M*.4 value:

```
0.1 × 2^4  =  0.1 × 16      =  1.6     →  stored as 1
1 / 2^4    =  1 / 16        =  0.0625
error      =  0.1 − 0.0625  =  0.0375  (37.5 %)
```

Radix 4 is too coarse for a value like 0.1 — the error is a
sizeable fraction of the value itself. Move the same number to
radix 16 and the picture changes:

```
0.1 × 2^16 =  0.1 × 65536  =  6553.6  →  stored as 6553
6553 / 65536                =  0.09999847...
error                       =  0.00000153  (< 0.002 %)
```

This behaviour isn't a bug — it is the same compromise IEEE-754
floating point makes with its mantissa. The difference is that a
float hides the trade-off behind a variable exponent, while
fixed-point puts it on a ledger that the programmer chooses up
front. The upside is predictability; the downside is that the
choice has to be made per variable.

A useful rule of thumb: pick the radix so that `2^−N` is at most
half the smallest step the application cares about. Any coarser
and small signals vanish; any finer and integer headroom is being
spent for no benefit.

A second consequence worth recording: quantisation error
*accumulates*. Summing a million low-radix values sums the errors
too. Signal-processing pipelines with long feedback paths are the
main reason to carry accumulators at a wider radix than the
individual samples — an idea the worked example later in this
primer will put into practice.

## Displaying a fixed-point value

The first thing most new code wants to do with a fixed-point value
is print it. That is where the absence of language support bites
immediately: `printf("%d", x)` shows the underlying integer, which
for the value 3.5 stored at radix 4 is `56`. Not very illuminating.

The standard pattern is to split the value into an integer part
and a fractional part, then format each one as a plain integer.
For a non-negative s*M*.*N* value `x`:

```c
s32 int_part  = x >> N;              /* same as FR2I(x, N) */
s32 frac_bits = x & ((1 << N) - 1);  /* the low N bits     */
```

The integer part prints directly with `%d`. The fractional part is
trickier: `frac_bits` is an integer in `[0, 2^N)`, which needs
rescaling to however many decimal digits are wanted after the
decimal point. Four digits is a reasonable default:

```c
/* Convert the fractional bits into a 0..9999 value by
 * multiplying up and then dividing by the radix scale.    */
s32 frac_dec = (frac_bits * 10000) >> N;

printf("%d.%04d\n", (int)int_part, (int)frac_dec);
```

For `x = 56` at radix 4, that prints `3.5000`. For `x = 23` at
radix 4 (which represents 1.4375), it prints `1.4375`. Note that
the final digit is truncated, not rounded: a real value of
`1.43756` would still print `1.4375`.

Negative values need one extra step. A bitwise mask on a negative
integer returns the two's-complement pattern of the low bits,
which doesn't correspond to the fractional part of the real
value. The cleanest workaround is to strip the sign first, format
the magnitude with the positive-value formula, and prepend the
minus sign by hand:

```c
void fr_show(s32 x, int radix)
{
    const char *sign = "";
    if (x < 0) { sign = "-"; x = -x; }

    s32 int_part  = x >> radix;
    s32 frac_bits = x & ((1 << radix) - 1);
    s32 frac_dec  = (frac_bits * 10000) >> radix;

    printf("%s%d.%04d", sign, (int)int_part, (int)frac_dec);
}
```

FR_Math ships this operation as
`FR_printNumF(f, n, radix, pad, prec)`. Instead of hard-coding
`printf`, the library version takes a per-character output
callback `f`, which makes it usable on targets without stdio — a
UART write, an LCD glyph pusher, a ring-buffer append. The `pad`
parameter sets a minimum field width and `prec` sets the number of
fractional digits. Rounding behaviour matches the hand-rolled
version: excess fractional digits are truncated, and negative
values are handled without the two's-complement trap described
above.

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
  doesn't fire. **Rounds to nearest** (adds
  0.5 LSB before the shift) — no clamp.
- `FR_FixMulSat(a, b, r)` — same shape with the
  same round-to-nearest, but
  saturates to `FR_OVERFLOW_POS` /
  `FR_OVERFLOW_NEG` if the result wouldn't
  fit. Prefer this one by default unless you've proven
  the product stays in range.

For the mixed-radix case — multiplying an
`s11.4` by an `s9.6` and landing the answer at
radix 8 — the same pattern applies: promote to `int64_t`,
multiply, and shift by `(ar + br - r)` to reach the
desired output radix.

### Division

```
a (sM1.N) / b (sM2.N)  →  c (s?.0)   ← the naive answer
```

Division is the operation where the naive answer is almost always
wrong. When two radix-*N* values are divided with plain integer
`/`, the fractional parts of numerator and denominator cancel each
other out and the result comes out at radix 0. That is
arithmetically correct but rarely what the caller wanted, because
the integer division then throws away every fractional bit of the
real-world answer.

To get a radix-*N* result out of two radix-*N* operands, the
numerator has to be pre-scaled by `2^N` first:

```
result = (a << N) / b
```

The shift recovers the *N* fractional bits the raw division was
about to discard. A concrete example, dividing `7.0` by `2.0` at
radix 4:

```c
s32 a = I2FR(7, 4);     /* 7.0 at s.4 = 112 */
s32 b = I2FR(2, 4);     /* 2.0 at s.4 = 32  */

/* Naive: 112 / 32 = 3, which stored at radix 4 represents
 * 0.1875.  Wrong by nearly a factor of 20.                */
s32 wrong = a / b;

/* Correct: pre-scale the numerator by 2^4 first. */
s32 right = ((s64)a << 4) / b;
/* (112 * 16) / 32 = 1792 / 32 = 56 = 3.5 at s.4            */
```

Three things to watch for:

- **Overflow of the shifted numerator.** Shifting an
  s*M*.*N* value left by *N* adds *N* bits to the register. If
  the numerator is already near the top of its range, that shift
  wraps. Carrying the numerator in an `int64_t` before the shift
  (as the example above does) avoids the trap on anything up to
  radix 31.
- **Division by zero.** C does not check for it at runtime; on
  most targets it either faults or produces silent garbage. Code
  that accepts a divisor from outside the function has to check
  it explicitly before the divide.
- **Rounding toward zero.** C's integer division truncates toward
  zero for both signs, so `−7 / 2 == −3` (not `−4`). Fixed-point
  division inherits that behaviour. Round-to-nearest can be
  layered on top by adding `b / 2` (for a positive numerator) or
  `−b / 2` (for a negative numerator) to the pre-scaled numerator
  before the divide.

FR_Math provides `FR_DIV(x, xr, y, yr)` which expands to
`((s64)(x) << (yr)) / (s32)(y)`. The numerator is promoted to
a 64-bit intermediate before the shift, so the full Q16.16 range
works correctly without overflow. For tiny targets where 64-bit
ops are too expensive (PIC, AVR, 8051), `FR_DIV32(x, xr, y, yr)`
is the 32-bit-only path — it requires `|x| < 2^(31 − yr)` to
avoid overflow in the intermediate shift. For the complementary
remainder, `FR_MOD(x, y)` expands to `(x) % (y)` with standard
C truncation-toward-zero semantics.

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

### Rounding on the way back down

Every operation that drops fractional bits — changing radix down,
bringing a multiply result back to a narrower type, converting
back to a plain integer — has to decide what to do with the bits
being thrown away. C's right-shift operator on signed values
truncates toward *negative* infinity, which surprises code that
expected it to truncate toward zero. The effect is subtle but
persistent:

```c
s32 half     =  8;    /*  0.5 at s.4 */
s32 neg_half = -8;    /* -0.5 at s.4 */

printf("%d\n", half     >> 4);  /*  0 */
printf("%d\n", neg_half >> 4);  /* -1, not 0 */
```

The negative case truncates *downward*: −0.5 becomes −1. For
display code and for anything that compares magnitudes across
sign, this is a common source of off-by-one bugs — the positive
and negative versions of the same magnitude round in opposite
directions.

The standard fix is round-half-away-from-zero: add half of the
shift amount before the shift, with the sign chosen to match the
value.

```c
/* Round-to-nearest from radix r back to integer. */
s32 bias   = (1 << (r - 1));
s32 result = (x >= 0) ? ((x + bias) >> r)
                      : -(((-x) + bias) >> r);
```

That form handles both signs symmetrically: 0.5 rounds to 1, −0.5
rounds to −1, 0.4 rounds to 0, −0.4 rounds to 0. The library
macro `FR_INT` takes a slightly different approach — it truncates
toward zero rather than toward negative infinity, which matches
C's `/` operator and is often easier to reason about. Which
rounding mode is right depends on the downstream consumer; for
display code, round-half-away-from-zero is almost always what a
human reader expects.

A further rounding mode, round-half-to-even ("banker's
rounding"), avoids statistical bias in long-running accumulators
but costs an extra branch per operation. The library does not
include it by default. Code that needs it can build it on top of
the raw shift.

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
   (aliases `INT32_MIN`, i.e. `0x80000000`) instead of wrapping.
   Think of saturation as the DSP convention: if the answer
   won't fit, return the nearest value that will. Better to
   lose precision at the extremes than to wrap a big positive
   number into a big negative one.
3. **Sentinel returns from functions that can fail.**
   A handful of functions have inputs that are simply not in
   their domain: `FR_sqrt(-1)`, `FR_log2(0)`,
   `FR_asin(x)` with `|x| > 1`, and so
   on. These return `FR_DOMAIN_ERROR`
   (`INT32_MIN`, i.e. `0x80000000` — the same value as
   `FR_OVERFLOW_NEG`). You check for it with a plain
   `if`.

In practice: call the saturating variants by default, check for
`FR_DOMAIN_ERROR` after any function whose input you
don't fully control, and in release builds on well-bounded
inputs the defensive paths never trigger. They're there for
the one time you feed `-0.0000001` into a square root at
3 AM.

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

## A worked example: one-pole IIR low-pass filter

The sections up to this point have introduced the pieces
individually: scaling, notation, quantisation, arithmetic,
overflow, and radix choice. A small end-to-end example is the
fastest way to see how those pieces fit together on a real
pipeline. The filter walked through below is a single-pole
infinite-impulse-response (IIR) low-pass — about the simplest
entry in the DSP catalogue, but realistic enough to exercise
nearly every decision the primer has covered so far.

In floating point, the filter is one line of arithmetic:

```c
/* Reference implementation, float. */
float y = 0.0f;

void step(float x)
{
    y = y + alpha * (x - y);
}
```

`alpha` controls how fast the filter tracks its input. Values
close to 1 make the output follow the input almost immediately;
values close to 0 smooth hard and react slowly. For this
walkthrough `alpha = 0.05`, a reasonably slow filter that takes
about 20 samples to reach roughly 63 % of a step input.

### Step 1: inventory the ranges

Three variables carry state through the loop. Each needs its
range and its required precision identified before any radix can
be picked:

- `x` — the input sample. Assumed here to arrive as a 16-bit
  signed audio sample in the range ±32767. That fixes one sign
  bit plus 15 integer bits, or in s*M*.*N* terms, `s15.0`.
- `alpha` — the filter coefficient. By construction
  `0 ≤ alpha ≤ 1`, so it has zero integer bits. The entire
  coefficient lives in the fractional part.
- `y` — the filter state. It represents a filtered version of
  `x`, so it shares the same ±32767 output range. But because it
  accumulates small updates on every sample, it will drift and
  lose precision unless carried at a higher radix than the raw
  input. This is the quantisation-error accumulation noted
  earlier in the primer, showing up in practice.

### Step 2: pick the radixes

The goal is to keep enough fractional precision in the state that
a single-LSB update does not get lost, while keeping every
intermediate product inside a 32-bit register.

- `x` stays at radix 0 (`s15.0`) — it arrives from the ADC that
  way and shifting it at the input costs memory for no benefit.
- `alpha` goes to radix 15 (`s0.15`). With 15 fractional bits
  the smallest representable coefficient is
  `1/32768 ≈ 0.0000305`, which is three orders of magnitude
  finer than the chosen `alpha = 0.05`. Plenty of resolution.
- `y` is carried internally at radix 15 (`s16.15`). That is 16
  integer bits (the full ±32767 range plus one sign bit) and 15
  fractional bits to absorb the small per-sample updates. Total:
  32 bits — fits an `s32` exactly, with no headroom spent on
  anything unnecessary.

### Step 3: walk through one sample

```c
/* Filter state held at radix 15. Persists across calls. */
static s32 y_r15 = 0;

/* alpha = 0.05 at radix 15:  0.05 * 32768 = 1638.4 -> 1638 */
#define ALPHA_R15  1638

/* One filter tick. x arrives as a raw s16 audio sample. */
s16 iir_step(s16 x)
{
    /* Bring x up to the state's radix for the subtraction.
     * Cast to s32 before shifting so the shift happens in
     * 32-bit space, not 16-bit.                           */
    s32 x_r15 = (s32)x << 15;

    /* delta = x - y, both at radix 15. No fractional bits
     * grow, and the s32 range is just wide enough to hold
     * the worst-case signed difference of two s16.15
     * values.                                             */
    s32 delta_r15 = x_r15 - y_r15;

    /* alpha * delta. delta is s16.15, alpha is s0.15, so
     * the raw product is s16.30 -- does not fit in 32 bits.
     * Promote to int64 for the multiply, shift off the
     * extra 15 fractional bits, and the result lands back
     * at radix 15.                                        */
    s64 prod       = (s64)delta_r15 * (s64)ALPHA_R15;
    s32 update_r15 = (s32)(prod >> 15);

    /* Accumulate. Both sides are at radix 15, the sum is at
     * radix 15, and as long as delta stays inside the
     * ALPHA-scaled range the accumulator never saturates. */
    y_r15 = y_r15 + update_r15;

    /* Output: round-half-away-from-zero back down to s15.0. */
    s32 bias = (y_r15 >= 0) ? (1 << 14) : -(1 << 14);
    return (s16)((y_r15 + bias) >> 15);
}
```

### Step 4: what each line actually defends against

- The `(s32)x << 15` line casts before shifting. Without the
  cast the shift would happen in 16-bit space and overflow on
  the very first sample.
- The `(s64)` promotion on the multiply forces the product to be
  computed in 64-bit. Without it, the 32-bit intermediate
  overflows whenever the input and state differ by more than
  roughly 40 LSB of 16-bit output — far below normal audio
  levels. The promotion is required, not optional.
- The `bias` term on the output is round-half-away-from-zero
  rather than truncate-toward-negative-infinity. Without it, the
  output carries a systematic negative bias of up to one LSB,
  which a DC-sensitive downstream stage (a meter, an integrator,
  a feedback loop) may care about.

### Step 5: test against the reference

Correctness of a fixed-point port is judged by running it
alongside the floating-point reference on the same input stream
and recording the maximum absolute difference. A simple harness
feeds both versions a few thousand samples — a mix of sine tones,
step inputs, and silence is enough to exercise the relevant paths
— and reports the worst-case delta. For a radix-15 one-pole IIR
the expected worst-case difference is on the order of a few LSB,
comparable to the inherent quantisation of the 16-bit output
format and not audible in normal listening. Anything substantially
larger indicates a radix choice that is too tight, a rounding
mode that is drifting, or a missing int64 promotion on the
multiply.

That is the full recipe: identify the ranges, pick radixes with
the register budget in mind, write the operations with explicit
intermediate types, choose an appropriate rounding mode on the
output, and cross-check against a float reference. Every other
FR_Math pipeline follows the same template, only with more
variables.

## FR_Math's naming conventions

A tour of the casing, because it tells you something about the
generation of each symbol:

| Prefix | What it is | Example |
| --- | --- | --- |
| `FR_XXX()` | `UPPERCASE` macro — inline, zero call overhead. | `FR_ADD`, `FR_ABS`, `FR2I` |
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
- `FR2I(x, r)` expands to
  `(x) >> (r)` — the reverse. Careful:
  C's right shift on signed values truncates toward
  −∞, so `FR2I(-1, 4)` is
  `-1`, not `0`. If you want
  round-to-nearest (which is usually what you want for display),
  add half an LSB before shifting: `(x + (1 << (r - 1))) >> r`.

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
