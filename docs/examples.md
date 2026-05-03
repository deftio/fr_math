# Examples

Short, runnable snippets for the most common FR_Math tasks. Each
example compiles cleanly against the v2.0.0 library with:

```bash
cc -Isrc example.c src/FR_math.c -o example
./example
```

No `-lm` needed — nothing in FR_Math links the C
math library. The 2D example additionally needs
`src/FR_math_2D.cpp` and a C++ compiler
(`c++` or `g++`).

## 1. Basic radix conversion

This first example walks through the three foundational moves:
construct a fixed-point number from an integer literal, change its
radix without losing data, and round-trip back. It's the
simplest thing you can do with the library and it exercises the
`I2FR`, `FR2I`, and `FR_CHRDX`
macros.

*Caveats:* `I2FR` and `FR2I` are
pure shift macros — they do no range-checking. Passing a
value that overflows the integer-part bits will silently wrap.
`FR_CHRDX` going from a higher radix to a lower one
throws away the lowest bits, so the round-trip here is only
lossless because we go up first.

```c
#include <stdio.h>
#include "FR_defs.h"
#include "FR_math.h"

int main(void)
{
    /* 3.5 at radix 4 = (3 << 4) + (0.5 * 16) = 48 + 8 = 56 = 0x38 */
    s32 a = I2FR(3, 4) | (1 << 3);       /* 3.5  at radix 4 */

    printf("a = %d (raw) = %d + %d/16\n",
           (int)a, (int)FR2I(a, 4), (int)(a & 0xF));

    /* Same 3.5 at radix 8: shift up by 4. */
    s32 a_r8 = FR_CHRDX(a, 4, 8);
    printf("a at radix 8 = %d\n", (int)a_r8);    /* 56 << 4 = 896 */

    /* Change back to radix 4 (no precision lost because 4 < 8). */
    s32 a_r4 = FR_CHRDX(a_r8, 8, 4);
    printf("round-trip = %d (equals a: %s)\n",
           (int)a_r4, a_r4 == a ? "yes" : "no");
    return 0;
}
```

## 2. Trig — integer degrees vs radian vs BAM

FR_Math supports three angle conventions and this example hits
all three: fixed-point degrees through the
`fr_sin_deg` / `fr_cos_deg` API, the radian-native
`fr_sin` / `fr_cos` (radian at a chosen
input radix), and BAM-native `fr_sin_bam` /
`fr_cos_bam`. All three paths feed the same 129-entry
quadrant cosine table under the hood and should produce nearly
identical results.

*Caveats:* the `radix` parameter on `fr_sin_deg(deg, radix)` is
the radix of the *degree input*, not the output. All sin/cos
functions return **s15.16** — that is, `s32` at radix 16,
where 1.0 = 65536 (`FR_TRIG_ONE`). The values compared below
are all in the same s15.16 format.

```c
#include <stdio.h>
#include "FR_math.h"

int main(void)
{
    /* Integer-degree legacy API — returns s15.16 */
    s32 s30_deg = FR_SinI(30);
    s32 c60_deg = FR_CosI(60);

    /* BAM-native core — same s15.16 output */
    s32 s30_bam = fr_sin_bam(FR_DEG2BAM(30));
    s32 c60_bam = fr_cos_bam(FR_DEG2BAM(60));

    /* Radian-native: input is radians at whatever radix you pass.
       Here we use FR_kPI at radix 16 to build pi/6 = 30 degrees. */
    s32 pi_over_6 = FR_kPI / 6;             /* radix 16 */
    s32 s30_rad   = fr_sin(pi_over_6, 16);  /* s15.16 out */

    printf("FR_SinI(30)     = %d   (s15.16)\n", s30_deg);
    printf("fr_sin_bam(30deg)= %d   (s15.16)\n", s30_bam);
    printf("fr_sin(pi/6)    = %d   (s15.16)\n", s30_rad);
    printf("FR_CosI(60)     = %d   (s15.16)\n", c60_deg);
    printf("expected 0.5    = %d   (1 << 15)\n", 1 << 15);
    return 0;
}
```

All four answers should land within a couple of LSBs of
`1 << 15 = 32768`, which is 0.5 in s15.16.

## 3. Square root and hypotenuse

This example shows the integer-only root family. Both
`FR_sqrt` and `FR_hypot` take a radix and
return at the **same** radix — no mixed-radix
output to worry about. `FR_hypot` is overflow-safe even
when `x² + y²` would overflow a 32-bit
intermediate: it promotes internally to `int64_t`.

*Caveats:* both functions return
`FR_DOMAIN_ERROR` (which is
`0x80000000`, the same bit pattern as
`FR_OVERFLOW_NEG`) on a negative input. Test against
the named sentinel explicitly, **not** with a
`< 0` test.

```c
#include <stdio.h>
#include "FR_math.h"

int main(void)
{
    const u16 r = 16;

    /* sqrt(2) at radix 16 */
    s32 two  = I2FR(2, r);
    s32 root = FR_sqrt(two, r);
    printf("sqrt(2) raw = 0x%x  (expect ~0x16a09 = 1.41421)\n",
           (unsigned)root);

    /* hypot(3, 4) = 5 exactly */
    s32 three = I2FR(3, r);
    s32 four  = I2FR(4, r);
    s32 h     = FR_hypot(three, four, r);
    printf("hypot(3,4)  = %d (integer part)\n", (int)FR2I(h, r));

    /* Domain-error handling */
    s32 bad = FR_sqrt(-1, r);
    if (bad == FR_DOMAIN_ERROR)
        printf("sqrt(-1) rejected, good.\n");

    /* Fast approximate — no multiply, no 64-bit math */
    s32 h_fast8 = FR_hypot_fast8(three, four);    /* 8-seg, ~0.14% err */
    printf("hypot_fast8(3,4) = %d (integer part)\n", (int)FR2I(h_fast8, r));
    return 0;
}
```

## 4. Logarithm, exponential, decibels

A realistic embedded-audio use case: turn a linear amplitude
into dB so you can drive a VU meter with a log scale. This
exercises `FR_log10` (the genuine mixed-radix logarithm
— input radix and output radix are independent), and the
macro `FR_POW10` (note: it's a macro, not a
function, because it reduces to `FR_pow2` after a
shift-only rescale).

*Caveats:* `FR_log10` takes
**three** arguments: value, input radix, output
radix. Passing two will not compile. The conversion constant
`20` for amplitude-to-dB is multiplied by shifting
because `20 = 16 + 4`, which is cheaper than calling
`FR_FixMuls` on a tiny constant.

```c
#include <stdio.h>
#include "FR_math.h"

/* Convert a linear amplitude (radix 16) into dB at radix 8. */
static s32 amplitude_to_db(s32 amp_r16)
{
    /* log10 output at radix 8 gives us ~0.004 dB resolution */
    s32 l10 = FR_log10(amp_r16, 16, 8);

    /* Multiply by 20: since 20 = 16 + 4, we can do two shifts + add. */
    return (l10 << 4) + (l10 << 2);
}

int main(void)
{
    s32 half = I2FR(1, 16) >> 1;   /* 0.5 at radix 16 */
    s32 db   = amplitude_to_db(half);
    printf("0.5 -> %d.%d dB  (expect ~ -6.02)\n",
           (int)FR2I(db, 8),
           (int)(FR_FRAC(db, 8) * 100 >> 8));

    /* 10^(0.3) is approximately 2.0 */
    s32 x = FR_POW10(FR_NUM(0, 3, 1, 16), 16);
    printf("10^0.3 = 0x%x (expect ~0x20000 = 2.0 at radix 16)\n",
           (unsigned)x);
    return 0;
}
```

## 5. Arctangent and atan2

The inverse-trig functions in FR_Math return angles in
**degrees**, not radians — the output fits in
an `s16` and you can feed it straight back into
`FR_SinI` / `FR_CosI` without any
conversion. This example exercises both `FR_atan`
(single-argument ratio) and `FR_atan2` (full-circle,
two-argument).

*Caveats:* `FR_atan2` takes only two
arguments (`y`, `x`) and has no radix
parameter — it returns degrees in [−180, 180] as
`s16`. The `radix` argument on
`FR_atan` is the radix of the *input* ratio,
not of the output.

```c
#include <stdio.h>
#include "FR_math.h"

int main(void)
{
    const u16 r = 14;

    /* atan(1) = 45 degrees */
    s16 a = FR_atan(I2FR(1, r), r);
    printf("atan(1) = %d degrees (expect 45)\n", a);

    /* Full-circle atan2 */
    s16 q2 = FR_atan2(I2FR( 1, r), I2FR(-1, r));  /*  135 deg */
    s16 q3 = FR_atan2(I2FR(-1, r), I2FR(-1, r));  /* -135 deg */
    printf("atan2( 1,-1) = %d\n", q2);
    printf("atan2(-1,-1) = %d\n", q3);

    /* asin with out-of-domain input */
    s16 bad = FR_asin(I2FR(2, r), r);
    if (bad == FR_DOMAIN_ERROR)
        printf("asin(2) rejected, good.\n");
    return 0;
}
```

## 6. Wave generators + phase accumulator

A 16-sample snapshot of a 1 kHz triangle wave at a
48 kHz sample rate, produced by the canonical NCO
(numerically controlled oscillator) pattern: a `u16`
phase that wraps naturally at `2^16`, driven
by a per-sample increment from `FR_HZ2BAM_INC`.

*Caveats:* the wave functions are stateless, so you can
drive as many voices as you like from independent phase
accumulators. `fr_wave_tri` is bipolar in s0.15 but
`fr_wave_tri_morph` is *unipolar* — watch
the sign convention if you mix them. The phase accumulator must
be `u16` for the free modular wrap to work.

```c
#include <stdio.h>
#include "FR_math.h"

int main(void)
{
    const u32 SR   = 48000;
    const u32 FREQ = 1000;

    u16 step = FR_HZ2BAM_INC(FREQ, SR);   /* ~1365 */
    u16 phase = 0;

    for (int i = 0; i < 16; i++) {
        s16 tri = fr_wave_tri(phase);
        s16 sqr = fr_wave_sqr(phase);
        printf("%2d: phase=%5u  tri=%6d  sqr=%6d\n",
               i, phase, tri, sqr);
        phase += step;            /* wraps at 2^16 for free */
    }
    return 0;
}
```

## 7. ADSR envelope

A linear-segment Attack-Decay-Sustain-Release envelope, the
bread-and-butter of synth voices. The envelope is sample-rate
agnostic — durations are in *samples*, not seconds
— so the same code works at 1 kHz or 192 kHz. The
internal level is held in s1.30, so even a 2-second attack at
48 kHz gets a non-zero per-sample increment.

*Caveats:* the sustain level is an `s16` in
s0.15 format — pass a value in [0, 32767], not a raw
register count. You must call `fr_adsr_init` once to
set the times, `fr_adsr_trigger` for note-on, and
`fr_adsr_release` for note-off. The envelope value
returned by `fr_adsr_step` is unipolar —
multiply it by your bipolar oscillator to get the amplitude-
modulated voice.

```c
#include <stdio.h>
#include "FR_math.h"

int main(void)
{
    fr_adsr_t env;
    fr_adsr_init(&env,
                 /* attack  */ 100,
                 /* decay   */ 200,
                 /* sustain */ 16384,          /* 0.5 in s0.15 */
                 /* release */ 400);

    fr_adsr_trigger(&env);

    for (int i = 0; i < 500; i++) {
        s16 y = fr_adsr_step(&env);
        if (i % 50 == 0) printf("t=%3d  amp=%d\n", i, y);
    }

    fr_adsr_release(&env);

    for (int i = 0; i < 500; i++) {
        s16 y = fr_adsr_step(&env);
        if (i % 50 == 0) printf("rel t=%3d  amp=%d\n", i, y);
        if (env.state == FR_ADSR_IDLE) {
            printf("envelope finished at rel t=%d\n", i);
            break;
        }
    }
    return 0;
}
```

## 8. 2D transforms

This example builds a 2D affine transform from rotation and
translation and uses it to map a point. `FR_Matrix2D_CPT`
is the "coordinate point transform" class from
`FR_math_2D.h`. It stores only the top two rows of the
affine matrix — the bottom row is always
`[0, 0, 1]`, so the class saves both memory and
multiplies.

*Caveats:* the class name is `FR_Matrix2D_CPT`
(the trailing `T` is part of the name). The point
transforms use explicit `x`, `y` arguments
and output pointers — there is no packaged
`Point` struct. Set the radix at construction; the
default is `FR_MAT_DEFPREC = 8`.

```cpp
#include <stdio.h>
#include "FR_math.h"
#include "FR_math_2D.h"

int main()
{
    FR_Matrix2D_CPT M(16);      /* work at radix 16 */

    /* Rotate 30 degrees around the origin, then translate by (100, 50). */
    M.setrotate(30);
    M.XlateRelativeI(100, 50);  /* uses the matrix's own radix */

    /* Transform the point (10, 0) integer-in / fixed-out at the matrix radix. */
    s32 xp, yp;
    M.XFormPtI((s32)10, (s32)0, &xp, &yp);
    printf("transformed = (%d, %d)  (integer at radix 16)\n",
           (int)FR2I(xp, 16), (int)FR2I(yp, 16));

    /* Invert and round-trip the transformed point back */
    if (M.inv()) {
        s32 bx, by;
        M.XFormPtI(xp, yp, &bx, &by);
        printf("round trip = (%d, %d)\n",
               (int)FR2I(bx, 16), (int)FR2I(by, 16));
    }
    return 0;
}
```

## 9. Formatted output without printf

On a deep-embedded target you often don't have
`printf` — pulling it in can cost 10 KB of
flash. FR_Math's `FR_printNumF` takes a
caller-supplied `putc` callback, so you can render a
fixed-point number straight to a UART or into a local buffer
without linking a single stdio symbol.

*Caveats:* the signature is
`FR_printNumF(putc, value, radix, pad, prec)` —
the sink comes **first**, not last. The sink is
`int (*)(char)`; return value is ignored. The sink
here writes into a static buffer; make sure the buffer is large
enough for the worst-case output.

```c
#include "FR_math.h"

/* Collect output into a local buffer so we don't need stdio. */
static char buf[64];
static int  cursor = 0;

static int sink(char c)
{
    if (cursor < (int)sizeof(buf) - 1) buf[cursor++] = c;
    return (int)(unsigned char)c;
}

int main(void)
{
    /* pi at radix 16 using the built-in constant */
    s32 x = FR_kPI;
    FR_printNumF(sink, x, 16, /* pad */ 0, /* prec */ 6);
    sink('\n');
    sink('\0');

    /* buf now contains the rendered number, no printf used. */
    return 0;
}
```

## 10. Integer-only 2D transform for scanline renderers

The `XFormPtI16` fast path takes `s16`
coordinates in and writes `s16` out. It's a tiny
bit lossier than the `s32` form, but it sidesteps all
the fixed-point conversion on the hot path — useful inside
the inner loop of a scanline rasterizer where you already know
your coordinates fit in 16 bits.

*Caveats:* the output is narrowed to `s16`,
so extreme inputs or large matrix entries can overflow silently.
The no-translate variant (`XFormPtI16NoTranslate`)
applies only the linear part and is useful for transforming
direction vectors rather than positions.

```cpp
#include <stdio.h>
#include "FR_math.h"
#include "FR_math_2D.h"

int main()
{
    FR_Matrix2D_CPT M;           /* default radix 8 */
    M.setrotate(45);

    s16 xp, yp;
    M.XFormPtI16(100, 0, &xp, &yp);  /* expect ~(71, 71) */
    printf("rotated (100, 0) by 45 deg = (%d, %d)\n", xp, yp);
    return 0;
}
```

## 11. String round-trip and radix precision

`FR_numstr` parses a decimal string into a fixed-point value;
`FR_printNumF` renders a fixed-point value back to a decimal
string. Together they form a symmetric pair — the natural
I/O layer for config files, serial protocols, or any situation
where a human-readable number must survive a round trip through
fixed-point representation.

The key insight is that **the radix controls how many
fractional bits you have**, and that directly determines how
many decimal digits survive the round trip faithfully. The
rule of thumb: every ~3.32 bits of radix give you one reliable
decimal digit (`log10(2) ≈ 0.301`, so `1 / 0.301 ≈ 3.32`).

| Radix | Fractional bits | Reliable decimal digits | Smallest step |
| --- | --- | --- | --- |
| 8 | 8 | ~2 | 1/256 ≈ 0.0039 |
| 16 | 16 | ~4–5 | 1/65536 ≈ 0.0000153 |
| 24 | 24 | ~7 | 1/16777216 ≈ 0.0000001 |

This example parses the same string `"3.14159265"` at three
different radixes and prints it back, so you can see exactly
where each radix runs out of precision.

```c
#include <stdio.h>
#include "FR_math.h"

/* Buffer-based putc for FR_printNumF — collects output in buf[]. */
static char buf[64];
static int  pos;
static int buf_putc(char c) { buf[pos++] = c; buf[pos] = '\0'; return 0; }

int main(void)
{
    const char *input = "3.14159265";

    printf("Round-trip: \"%s\" through three radixes\n\n", input);
    printf("  radix  bits  parse-hex   printed-back\n");
    printf("  -----  ----  ---------   ---------------\n");

    /* ---- Radix 8: only ~2 reliable decimal digits ---- */
    {
        s32 val = FR_numstr(input, 8);
        pos = 0;
        FR_printNumF(buf_putc, val, 8, 0, 8);
        printf("     8      8  0x%08x  %s\n", (unsigned)val, buf);
        /* Expected: "3.14062500" — digits after the 2nd are
         * unreliable because 8 bits can only distinguish 256
         * fractional levels. 0.14159 rounds to 36/256 ≈ 0.140625. */
    }

    /* ---- Radix 16: ~4–5 reliable decimal digits ---- */
    {
        s32 val = FR_numstr(input, 16);
        pos = 0;
        FR_printNumF(buf_putc, val, 16, 0, 8);
        printf("    16     16  0x%08x  %s\n", (unsigned)val, buf);
        /* Expected: "3.14158630" — good through 5 digits, then
         * quantization noise appears.  This is the sweet spot for
         * most embedded work: 16 bits of fraction fits in an s32
         * with 15 bits of integer range (±32767). */
    }

    /* ---- Radix 24: ~7 reliable decimal digits ---- */
    {
        s32 val = FR_numstr(input, 24);
        pos = 0;
        FR_printNumF(buf_putc, val, 24, 0, 8);
        printf("    24     24  0x%08x  %s\n", (unsigned)val, buf);
        /* Expected: "3.14159262" — good through 7+ digits.
         * The trade-off: with 24 fractional bits you only have
         * 7 integer bits (±127), so the range is much smaller. */
    }

    /* ---- Practical guidance ----
     *
     * Choose your radix based on the precision you need to
     * preserve through a round trip:
     *
     *   - Config file says "gain = 0.75"?  Radix 8 is plenty.
     *   - Sensor calibration "offset = 1.2345"?  Radix 16.
     *   - Scientific constant "k = 0.0000056"?  Radix 24,
     *     or even radix 28 if you only need a small integer range.
     *
     * FR_numstr + FR_printNumF handle the string side;
     * FR_CHRDX handles radix changes in between.
     */

    return 0;
}
```

Expected output:

```
Round-trip: "3.14159265" through three radixes

  radix  bits  parse-hex   printed-back
  -----  ----  ---------   ---------------
     8      8  0x00000324  3.14062500
    16     16  0x0003243f  3.14158630
    24     24  0x03243f6a  3.14159262
```

The hex column makes the bit-level reality visible:
at radix 8 the value is `0x324` — only 10 significant bits —
so the decimal rendering can only faithfully reproduce about two
fractional digits. At radix 24 the value is `0x03243F6A` — 26
significant bits — and seven decimal digits survive. The
eighth digit (`5` vs `4`) shows the quantization floor: `2^−24 ≈
6 × 10^−8`, so the last digit is always uncertain.

## See also

- [API Reference](api-reference.md) — full
  symbol table.
- [Fixed-Point Primer](fixed-point-primer.md)
  — background on why the code looks this way.
- [Building & Testing](building.md) — how
  to compile, test, and cross-compile.
