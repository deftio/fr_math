#!/usr/bin/env python3
"""Generate gFR_ATAN_TAB[65] for FR_atan2.

Entry i approximates atan(i/64) in radians at u0.16. FR_atan2 looks up
t = min/max (u0.16, in [0, 1]) with linear interpolation:

    idx = t >> 10, fr = t & 1023
    a = tab[idx] + (((tab[idx+1] - tab[idx]) * fr + 512) >> 10)

Plain rounding of atan(i/64) leaves a peak error of about 2.07 LSB: atan
is concave on [0, 1], so every chord sits below the curve. This script
starts from the rounded values and nudges each interior entry by up to
+/-3 LSB (coordinate descent) to minimise the peak error of the exact
integer lerp above, measured on all 65537 values of t. Entries 0 and 64
stay exact (0 and pi/4) so the octants meet without a step at 45 degrees.

Paste the output into FR_math.c.
"""
import math

N = 64
STEP = 65536 // N
REF = [math.atan(t / 65536.0) * 65536.0 for t in range(65537)]


def lerp(tab, t):
    idx, fr = t >> 10, t & (STEP - 1)
    if idx >= N:
        return tab[N]
    lo, hi = tab[idx], tab[idx + 1]
    return lo + (((hi - lo) * fr + 512) >> 10)


def seg_err(tab, k):
    end = min((k + 1) * STEP, 65536)
    return max(abs(lerp(tab, t) - REF[t]) for t in range(k * STEP, end + 1))


def peak(tab):
    return max(seg_err(tab, k) for k in range(N))


tab = [round(math.atan(i / N) * 65536) for i in range(N + 1)]
plain = peak(tab)

improved = True
while improved:
    improved = False
    for i in range(1, N):
        segs = (i - 1, i)
        base = tab[i]
        best = max(seg_err(tab, k) for k in segs)
        for d in (-3, -2, -1, 1, 2, 3):
            tab[i] = base + d
            e = max(seg_err(tab, k) for k in segs)
            if e < best - 1e-9:
                best, base, improved = e, base + d, True
        tab[i] = base

print(f"static const u16 gFR_ATAN_TAB[{N + 1}] = {{")
for row in range(0, N + 1, 8):
    vals = ", ".join(f"{v:5d}" for v in tab[row:row + 8])
    comma = "," if row + 8 <= N else ""
    print(f"\t{vals}{comma}")
print("};")
print(f"\n/* Peak lookup error: rounded {plain:.2f} LSB, tuned {peak(tab):.2f} LSB (radix 16). */")

assert tab[0] == 0
assert tab[N] == round(math.pi / 4 * 65536) == 51472
assert all(b > a for a, b in zip(tab, tab[1:])), "table must be increasing"
print("Verification passed.")
