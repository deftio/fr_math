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
