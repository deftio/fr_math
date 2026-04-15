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
