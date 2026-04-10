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

---

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

---

## 2. Trig — integer degrees vs radian vs BAM

FR_Math supports three angle conventions and this example hits
all three: integer degrees through the legacy
`FR_Sin` / `FR_Cos` API, the radian-native
`fr_sin` / `fr_cos` (radian at a chosen
input radix), and BAM-native `fr_sin_bam` /
`fr_cos_bam`. All three paths feed the same 129-entry
quadrant cosine table under the hood and should produce nearly
identical results.

*Caveats:* the three output formats are **not**
the same radix. `FR_Sin(deg, radix)` returns s0.15 (not
at the radix you passed in — that radix is the radix of
`deg`, not of the sine result). `fr_sin_bam`
returns s0.15 too. The radian-native form returns s0.15 as well.
So the raw values compared below are all in the same s0.15
format.

```c
#include <stdio.h>
#include "FR_math.h"

int main(void)
{
    /* Integer-degree legacy API — returns s0.15 */
    s16 s30_deg = FR_SinI(30);
    s16 c60_deg = FR_CosI(60);

    /* BAM-native core — same s0.15 output */
    s16 s30_bam = fr_sin_bam(FR_DEG2BAM(30));
    s16 c60_bam = fr_cos_bam(FR_DEG2BAM(60));

    /* Radian-native: input is radians at whatever radix you pass.
       Here we use FR_kPI at radix 16 to build pi/6 = 30 degrees. */
    s32 pi_over_6 = FR_kPI / 6;             /* radix 16 */
    s16 s30_rad   = fr_sin(pi_over_6, 16);  /* s0.15 out */

    printf("FR_SinI(30)     = %d   (s0.15)\n", s30_deg);
    printf("fr_sin_bam(30deg)= %d   (s0.15)\n", s30_bam);
    printf("fr_sin(pi/6)    = %d   (s0.15)\n", s30_rad);
    printf("FR_CosI(60)     = %d   (s0.15)\n", c60_deg);
    printf("expected 0.5    = %d   (1 << 14)\n", 1 << 14);
    return 0;
}
```

All four answers should land within a couple of LSBs of
`1 << 14 = 16384`, which is 0.5 in s0.15.

---

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
    return 0;
}
```

---

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
    s32 x = FR_POW10(FR_num(0, 3, 16), 16);
    printf("10^0.3 = 0x%x (expect ~0x20000 = 2.0 at radix 16)\n",
           (unsigned)x);
    return 0;
}
```

---

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

---

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

---

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

---

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

---

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

---

## 10. Integer-only 2D transform for scanline renderers

The `XFormPtI16` fast path takes `s16`
coordinates in and writes `s16` out. It's a tiny
bit lossier than the `s32` form, but it sidesteps all
the fixed-point conversion on the hot path — useful inside
the inner loop of a scanline rasteriser where you already know
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

---

## See also

- [API Reference](api-reference.md) — full
  symbol table.
- [Fixed-Point Primer](fixed-point-primer.md)
  — background on why the code looks this way.
- [Building & Testing](building.md) — how
  to compile, test, and cross-compile.
