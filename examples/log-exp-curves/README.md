# Log / Exp / Sqrt Curves

Sweeps the FR_Math logarithmic, exponential, and square-root functions,
printing comparison tables against IEEE 754 double-precision reference
values with per-point and summary error statistics.

## What it demonstrates

| Table | Functions tested | Input range |
|-------|-----------------|-------------|
| Log | `FR_log2`, `FR_ln`, `FR_log10` | 0.25 to 10.0 (9 selected points) |
| Exp | `FR_pow2`, `FR_EXP`, `FR_POW10` | -3.0 to 3.0 in 0.5 steps |
| Sqrt | `FR_sqrt` | 0.25 to 100.0 (15 selected points) |

Each table shows: `input | FR_result | reference | error%`

A summary line per function gives max |error%| and avg |error%|.

## Building

```bash
make            # compiles log_exp_curves
make run        # compiles and runs
make clean      # removes build artifacts
```

Or compile manually:

```bash
g++ -I../../src -Wall -Os log_exp_curves.cpp ../../src/FR_math.c -lm -o log_exp_curves
```

## Expected output

```
FR_Math — Log / Exp / Sqrt Curves  (v..., radix=16)

========================================
  Log functions (input > 0)
========================================

  input    | FR_log2      ref_log2     err%     | ...
  ...
  FR_log2   max |err|: ...   avg |err|: ...

========================================
  Exp functions (input -3.0 to 3.0)
========================================
  ...

========================================
  Square root (FR_sqrt)
========================================
  ...

--- end ---
```

## Dependencies

- A C++ compiler (g++ or clang++)
- FR_Math source (`../../src/FR_math.c`, `../../src/FR_math.h`, `../../src/FR_defs.h`)
- Standard C math library (`-lm`, for double-precision reference values)
