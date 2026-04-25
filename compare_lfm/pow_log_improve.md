# FR_math pow/log Accuracy Improvements

Proposed changes to FR_pow2, FR_log2, FR_EXP, FR_ln, FR_log10, and FR_POW10.
All changes are backward-compatible. No API changes. No new functions.

## Current Error Budget

Measured against IEEE 754 double as gold standard, Q16.16 (s15.16), 50k test
points. 1 LSB = 1/65536 = 1.53e-5.

| function | max error (LSB) | mean error (LSB) | bottleneck |
|----------|----------------:|------------------:|------------|
| exp      | 1979            | 114               | pow2 table (16 segments) |
| ln       | 37.6            | 4.4               | log2 table (32 segments) + shift-only scaling |
| log10    | 15.8            | 3.8               | log2 table + shift-only scaling |
| log2     | 53.2            | 4.7               | log2 table (32 segments) |
| pow2     | 2282            | 148               | pow2 table (16 segments) |

The error decomposition test (bench_explog.cpp) proved that the lookup tables
are the dominant error source, not the shift-only scaling macros. For exp,
the pow2 table alone accounts for 115% of the total error (the shift-only
scaling actually has a bias that partially cancels the table error).

## The Five Changes

### Change 1: Expand pow2 table from 17 to 65 entries

This is the big win. The current gFR_POW2_FRAC_TAB has 17 entries (16 linear
segments). Linear interpolation error on a convex function is O(h^2) where h is
the segment width. Going from 16 to 64 segments gives 16x smaller error.

Measured: pow2 table error drops from 2282 LSB to 210 LSB.

**Cost**: +192 bytes ROM (68 -> 260 bytes).

**Code change in FR_math.c**: Replace the table and change two constants in
FR_pow2 (index bits and interpolation bits).

Current:
```c
static const u32 gFR_POW2_FRAC_TAB[17] = {
     65536,  68438,  71468,  74632,  77936,  81386,  84990,  88752,
     92682,  96785, 101070, 105545, 110218, 115098, 120194, 125515,
    131072
};
```

New:
```c
static const u32 gFR_POW2_FRAC_TAB[65] = {
     65536,  66250,  66971,  67700,  68438,  69183,  69936,  70698,
     71468,  72246,  73032,  73828,  74632,  75444,  76266,  77096,
     77936,  78785,  79642,  80510,  81386,  82273,  83169,  84074,
     84990,  85915,  86851,  87796,  88752,  89719,  90696,  91684,
     92682,  93691,  94711,  95743,  96785,  97839,  98905,  99982,
    101070, 102171, 103283, 104408, 105545, 106694, 107856, 109031,
    110218, 111418, 112631, 113858, 115098, 116351, 117618, 118899,
    120194, 121502, 122825, 124163, 125515, 126882, 128263, 129660,
    131072
};
```

In FR_pow2, change the index/interpolation split from 4/12 to 6/10:

Current (line 489-491):
```c
    /* Top 4 bits index the table; bottom 12 are the interpolation fraction. */
    idx     = frac_full >> 12;
    frac_lo = frac_full & ((1L << 12) - 1);
```

New:
```c
    /* Top 6 bits index the table; bottom 10 are the interpolation fraction. */
    idx     = frac_full >> 10;
    frac_lo = frac_full & ((1L << 10) - 1);
```

And the interpolation line (494):

Current:
```c
    mant = lo + (((hi - lo) * frac_lo) >> 12);
```

New:
```c
    mant = lo + (((hi - lo) * frac_lo) >> 10);
```

That's it. Same algorithm, three numbers change (12->10, 12->10, [17]->[65]).
Update the comment at the top of FR_pow2 accordingly.


### Change 2: Replace shift-only scaling macros with single multiply

The current FR_EXP, FR_ln, and FR_log10 use shift-only macros (FR_SLOG2E,
FR_SrLOG2E, FR_SrLOG2_10) to convert between logarithmic bases. These have
~5-10 LSB error at Q16.16. After fixing the table (Change 1), these macros
become the new bottleneck for exp.

The fix: one 32x32->64 multiply with a high-precision constant at radix 28.

**Cost**: one multiply per call. Zero cost on ARM (single-cycle MUL + shift).
On 16-bit targets this is one software multiply — still cheap since exp/ln
are already expensive functions.

**Add to FR_math.h** (near the existing FR_kLOG2E definitions):

```c
/* High-precision scaling constants at radix 28.
 * Used by FR_EXP, FR_ln, FR_log10 for base conversion.
 * At radix 28 these have ~9 decimal digits of precision, far exceeding
 * the ~4.8 digits of Q16.16.
 */
#define FR_kLOG2E_28    (387270501)   /* log2(e)   = 1.4426950408889634  */
#define FR_krLOG2E_28   (186065279)   /* ln(2)     = 0.6931471805599453  */
#define FR_kLOG2_10_28  (891723283)   /* log2(10)  = 3.3219280948873622  */
#define FR_krLOG2_10_28  (80807124)   /* log10(2)  = 0.3010299956639812  */

/* Multiply fixed-point value x (any radix) by a radix-28 constant k.
 * Result stays at x's radix. Uses 64-bit intermediate.
 * Rounds to nearest (adds 0.5 LSB before shift).
 */
#define FR_MULK28(x, k) ((s32)((((int64_t)(x) * (int64_t)(k)) + (1 << 27)) >> 28))
```

**Overflow safety**: worst case is FR_MULK28(INT32_MAX, FR_kLOG2_10_28) =
2^31 * 891723283 = 2^31 * ~2^30 = 2^61, which fits in s64 (63 magnitude
bits) with 2 bits to spare. The multiply cannot overflow.

**Radix independence**: if x is at radix R, then x * k28 is at radix R+28,
and >>28 puts the result back at radix R. Works for any R.

**Update FR_EXP and FR_POW10 macros in FR_math.h**:

Current:
```c
#define FR_EXP(input, radix)   (FR_pow2(FR_SLOG2E(input), radix))
#define FR_POW10(input, radix) (FR_pow2(FR_SLOG2_10(input), radix))
```

New:
```c
#define FR_EXP(input, radix)   (FR_pow2(FR_MULK28((input), FR_kLOG2E_28), (radix)))
#define FR_POW10(input, radix) (FR_pow2(FR_MULK28((input), FR_kLOG2_10_28), (radix)))
```

**Update FR_ln and FR_log10 in FR_math.c**:

Current:
```c
s32 FR_ln(s32 input, u16 radix, u16 output_radix)
{
    s32 r = FR_log2(input, radix, output_radix);
    return FR_SrLOG2E(r);
}

s32 FR_log10(s32 input, u16 radix, u16 output_radix)
{
    s32 r = FR_log2(input, radix, output_radix);
    return FR_SrLOG2_10(r);
}
```

New:
```c
s32 FR_ln(s32 input, u16 radix, u16 output_radix)
{
    s32 r = FR_log2(input, radix, output_radix);
    return FR_MULK28(r, FR_krLOG2E_28);
}

s32 FR_log10(s32 input, u16 radix, u16 output_radix)
{
    s32 r = FR_log2(input, radix, output_radix);
    return FR_MULK28(r, FR_krLOG2_10_28);
}
```


### Change 3: Rename old shift-only macros as _FAST variants

The shift-only macros are still useful for targets where multiply is very
expensive (MSP430, 8-bit AVR). Keep them available under _FAST names.

**Add to FR_math.h** (keep existing macros, add aliases):

```c
/* Shift-only (multiply-free) base-conversion macros.
 * Lower accuracy (~5-10 LSB at Q16.16) but no multiply instruction.
 * Use these on targets where 32x32->64 multiply is expensive.
 */
#define FR_EXP_FAST(input, radix)   (FR_pow2(FR_SLOG2E(input), radix))
#define FR_POW10_FAST(input, radix) (FR_pow2(FR_SLOG2_10(input), radix))
```

The underlying FR_SLOG2E, FR_SrLOG2E, FR_SrLOG2_10, FR_SLOG2_10 macros
stay as-is. They are still used by the _FAST variants and may be useful
independently.

The existing FR_kLOG2E (94548) and FR_krLOG2E (45426) constants at radix 16
stay as-is. They are used in other contexts and cost nothing if unused.


### Change 4: Expand log2 table from 33 to 65 entries

Same pattern as the pow2 table expansion. The current gFR_LOG2_MANT_TAB has
33 entries (32 segments). Going to 65 entries (64 segments) gives ~4x smaller
interpolation error.

**Cost**: +128 bytes ROM (132 -> 260 bytes).

**Code change in FR_math.c**: Replace the table and change three constants in
FR_log2 (index bits and interpolation bits).

Current:
```c
static const u32 gFR_LOG2_MANT_TAB[33] = {
        0,  2909,  5732,  8473, 11136, 13727, 16248, 18704,
    21098, 23433, 25711, 27936, 30109, 32234, 34312, 36346,
    38336, 40286, 42196, 44068, 45904, 47705, 49472, 51207,
    52911, 54584, 56229, 57845, 59434, 60997, 62534, 64047,
    65536
};
```

New:
```c
static const u32 gFR_LOG2_MANT_TAB[65] = {
        0,  1466,  2909,  4331,  5732,  7112,  8473,  9814,
    11136, 12440, 13727, 14996, 16248, 17484, 18704, 19909,
    21098, 22272, 23433, 24579, 25711, 26830, 27936, 29029,
    30109, 31178, 32234, 33279, 34312, 35334, 36346, 37346,
    38336, 39316, 40286, 41246, 42196, 43137, 44068, 44990,
    45904, 46809, 47705, 48593, 49472, 50344, 51207, 52063,
    52911, 53751, 54584, 55410, 56229, 57040, 57845, 58643,
    59434, 60219, 60997, 61769, 62534, 63294, 64047, 64794,
    65536
};
```

In FR_log2, change the index/interpolation split from 5/25 to 6/24:

Current (line 586-587):
```c
    idx  = (s32)(m >> 25);                    /* 5 bits  */
    frac = (s32)(m & ((1u << 25) - 1));       /* 25 bits */
```

New:
```c
    idx  = (s32)(m >> 24);                    /* 6 bits  */
    frac = (s32)(m & ((1u << 24) - 1));       /* 24 bits */
```

And the interpolation line (590):

Current:
```c
    mant_log2 = lo + (s32)(((int64_t)(hi - lo) * frac) >> 25);
```

New:
```c
    mant_log2 = lo + (s32)(((int64_t)(hi - lo) * frac) >> 24);
```

Same algorithm, three numbers change (25->24, 25->24, [33]->[65]).
Update the comments at the top of FR_log2 accordingly. ln and log10
improve automatically since they call FR_log2 internally.


### Change 5: FR_DIV rounding

FR_DIV currently truncates, giving up to 1.0 LSB error. Adding round-to-
nearest brings this to 0.5 LSB, matching FR_FixMuls and FR_sqrt behavior.

FR_DIV already uses an s64 intermediate (from the earlier 32->64 bit fix),
so the rounding cost is one addition.

**Simple version** (correct for positive quotients, within 0.5 LSB for
negative quotients):

Current:
```c
#define FR_DIV(x, xr, y, yr) ((s32)(((s64)(x) << (yr)) / (s32)(y)))
```

New:
```c
#define FR_DIV(x, xr, y, yr) \
    ((s32)((((s64)(x) << (yr)) + ((s64)(y) >> 1)) / (s64)(y)))
```

This adds half the divisor before dividing. For positive quotients it rounds
to nearest exactly. For negative quotients C's truncation-toward-zero means
the bias is slightly off, but worst case remains 0.5 LSB.

**Exact version** (correct for all sign combinations, as a static inline):

```c
static inline s32 FR_div_rnd(s64 num, s32 den) {
    if ((num ^ den) >= 0)                   /* same sign: positive quotient */
        return (s32)((num + den / 2) / den);
    else                                     /* negative quotient */
        return (s32)((num - den / 2) / den);
}
#define FR_DIV(x, xr, y, yr) FR_div_rnd((s64)(x) << (yr), (s32)(y))
```

Rename the old truncating version:
```c
#define FR_DIV_TRUNC(x, xr, y, yr) ((s32)(((s64)(x) << (yr)) / (s32)(y)))
```

**Cost**: zero additional ROM (one s64 addition). The inline function version
adds a branch but the branch is perfectly predicted in practice.


## Expected Results

Combining all five changes, measured with bench_explog.cpp:

| function | before (LSB) | after (LSB) | improvement |
|----------|-------------:|------------:|-------------|
| exp      | 1979         | ~207        | ~10x better (table + scaling) |
| pow2     | 2282         | ~210        | ~11x better (table) |
| ln       | 37.6         | ~3          | ~12x better (table + scaling) |
| log10    | 15.8         | ~3          | ~5x better (table + scaling) |
| log2     | 53.2         | ~13         | ~4x better (table) |
| div      | 1.0          | ~0.5        | 2x better (rounding) |

Speed: no measurable change (tested on Apple M-series, 50k iterations).

Size delta: +192 bytes (pow2 table) + 128 bytes (log2 table) = +320 bytes ROM.
Zero RAM.


## Files Changed

| file | change |
|------|--------|
| src/FR_math.h | Add FR_kLOG2E_28, FR_krLOG2E_28, FR_kLOG2_10_28, FR_krLOG2_10_28 |
| src/FR_math.h | Add FR_MULK28 macro |
| src/FR_math.h | Update FR_EXP and FR_POW10 to use FR_MULK28 |
| src/FR_math.h | Add FR_EXP_FAST and FR_POW10_FAST (old behavior) |
| src/FR_math.h | Update FR_DIV to round-to-nearest |
| src/FR_math.h | Add FR_DIV_TRUNC (old truncating behavior) |
| src/FR_math.c | Replace gFR_POW2_FRAC_TAB[17] with [65] |
| src/FR_math.c | Change FR_pow2 index from 4-bit/12-bit to 6-bit/10-bit |
| src/FR_math.c | Replace gFR_LOG2_MANT_TAB[33] with [65] |
| src/FR_math.c | Change FR_log2 index from 5-bit/25-bit to 6-bit/24-bit |
| src/FR_math.c | Update FR_ln to use FR_MULK28 |
| src/FR_math.c | Update FR_log10 to use FR_MULK28 |


## Helper Scripts

### Generate the 65-entry pow2 table

```python
#!/usr/bin/env python3
"""Generate gFR_POW2_FRAC_TAB[65] for FR_pow2.

Output: 2^(i/64) at s.16 fixed point, for i = 0..64.
Paste directly into FR_math.c.
"""
import math

N = 64
entries = [round(2.0 ** (i / N) * 65536) for i in range(N + 1)]

print(f"static const u32 gFR_POW2_FRAC_TAB[{N+1}] = {{")
for row in range(0, N + 1, 8):
    chunk = entries[row:row+8]
    vals = ", ".join(f"{v:6d}" for v in chunk)
    comma = "," if row + 8 <= N else ""
    print(f"    {vals}{comma}")
print("};")
print(f"\n/* Size: {(N+1)*4} bytes.  Entry i = round(2^(i/{N}) * 65536). */")

# Verify
assert entries[0]  == 65536,  f"first entry should be 65536, got {entries[0]}"
assert entries[N]  == 131072, f"last entry should be 131072, got {entries[N]}"
assert entries[32] == 92682,  f"midpoint (2^0.5) should be 92682, got {entries[32]}"
print("Verification passed.")
```


### Generate the 65-entry log2 table

```python
#!/usr/bin/env python3
"""Generate gFR_LOG2_MANT_TAB[65] for FR_log2.

Output: log2(1 + i/64) at s.16 fixed point, for i = 0..64.
Paste directly into FR_math.c.
"""
import math

N = 64
entries = [round(math.log2(1.0 + i / N) * 65536) for i in range(N + 1)]

print(f"static const u32 gFR_LOG2_MANT_TAB[{N+1}] = {{")
for row in range(0, N + 1, 8):
    chunk = entries[row:row+8]
    vals = ", ".join(f"{v:5d}" for v in chunk)
    comma = "," if row + 8 <= N else ""
    print(f"    {vals}{comma}")
print("};")
print(f"\n/* Size: {(N+1)*4} bytes.  Entry i = round(log2(1 + i/{N}) * 65536). */")

# Verify
assert entries[0]  == 0,     f"first should be 0, got {entries[0]}"
assert entries[N]  == 65536, f"last should be 65536, got {entries[N]}"
assert entries[32] == 38336, f"midpoint log2(1.5) should be 38336, got {entries[32]}"
print("Verification passed.")
```


### Generate the radix-28 constants

```python
#!/usr/bin/env python3
"""Generate high-precision scaling constants at radix 28 for FR_math.h.

These are used by FR_MULK28 for base conversion in exp/ln/log10.
"""
import math

R = 28
scale = 2**R

constants = [
    ("FR_kLOG2E_28",    math.log2(math.e),  "log2(e)"),
    ("FR_krLOG2E_28",   math.log(2),         "ln(2)"),
    ("FR_kLOG2_10_28",  math.log2(10),       "log2(10)"),
    ("FR_krLOG2_10_28", math.log10(2),       "log10(2)"),
]

for name, exact, desc in constants:
    k28 = round(exact * scale)
    approx = k28 / scale
    err = abs(approx - exact)
    print(f"#define {name:20s} ({k28:>12d})   /* {desc:10s} = {exact:.16f} */")
    assert err < 1e-8, f"{name} error {err:.2e} exceeds 1e-8"

print()

# Overflow check: worst case is INT32_MAX * largest constant
worst_k = max(c[1] for c in constants)
worst_k28 = round(worst_k * scale)
product_bits = (2**31 * worst_k28).bit_length()
print(f"Overflow check: INT32_MAX * {worst_k28} needs {product_bits} bits (s64 has 63+sign)")
assert product_bits <= 62, "OVERFLOW RISK"
print("Overflow check passed.")
```


### Verify the changes end-to-end

After making the code changes, run the existing test suite to confirm nothing
broke, then run the comparison benchmark:

```bash
# From the repo root:
make clean && make test

# From .compare/ — rebuild against updated FR_math and run full comparison:
make clean && make run
# Output: comparison_results.json with all 13 functions vs libfixmath

# From .compare/ — detailed exp/log analysis:
make -f Makefile.explog clean
make -f Makefile.explog run
```


## Optional: FR_log2 CLZ Optimization

The current FR_log2 finds the leading bit position with a while loop:

```c
    u = (u32)input;
    p = 0;
    while (u > 1)
    {
        u >>= 1;
        p++;
    }
```

This could be replaced with __builtin_clz (GCC/Clang) or a manual binary
search for a constant-time alternative:

```c
    p = 31 - __builtin_clz((unsigned)input);
```

This saves ~15 iterations on average but only matters if FR_log2 is called in
a tight loop. Not part of the five changes above — can be done separately as
a minor speed optimization. Note: __builtin_clz is not portable to all
compilers, so a fallback would be needed for strict portability.
