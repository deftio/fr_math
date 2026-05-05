# Trig Accuracy — FR_Math vs libfixmath

Head-to-head trig accuracy comparison between FR_Math and
[libfixmath](https://github.com/PetteriAimworking/libfixmath),
using IEEE 754 double-precision as the reference.

## What it demonstrates

- Sweeps sin, cos, and tan over 0-360 degrees in 1-degree steps
- FR_Math calls: `FR_SinI(deg)`, `FR_CosI(deg)`, `FR_TanI(deg)` (integer degrees, s15.16 output)
- libfixmath calls: `fix16_sin`, `fix16_cos`, `fix16_tan` (fix16_t radians, Q16.16 output)
- Reference: `sin()`, `cos()`, `tan()` from `<cmath>` (IEEE 754 double)

## Output tables

**Detail table** (one row per degree):

```
  deg | FR_sin    LFM_sin   ref_sin    FR_err%  LFM_err% | (same for cos) | (same for tan)
```

**Summary table**:

```
  function   | FR_max%      FR_avg%      | LFM_max%     LFM_avg%
  -----------+--------------------------+---------------------------
  sin        | ...          ...          | ...          ...
  cos        | ...          ...          | ...          ...
  tan        | ...          ...          | ...          ...
```

## Building

This example requires the libfixmath source tree at
`../../compare_lfm/libfixmath/libfixmath/`.

```bash
make            # compiles trig_accuracy (checks for libfixmath)
make run        # compiles and runs
make clean      # removes build artifacts
```

Or compile manually:

```bash
LFM=../../compare_lfm/libfixmath/libfixmath
g++ -I../../src -I$LFM -Wall -Os \
    trig_accuracy.cpp ../../src/FR_math.c \
    $LFM/fix16.c $LFM/fix16_trig.c $LFM/fix16_sqrt.c $LFM/fix16_exp.c \
    -lm -o trig_accuracy
```

## Expected output

```
FR_Math vs libfixmath — Trig Accuracy Comparison  (v...)

  deg | FR_sin    LFM_sin   ref_sin  ...
  ----+-------------------------------...
    0 |  0.00000  0.00000  0.00000  ...
    1 |  0.01745  0.01745  0.01745  ...
  ...
  360 |  0.00000  0.00000 -0.00000  ...

  ============================================================
  Summary
  ============================================================

  function   | FR_max%      FR_avg%      | LFM_max%     LFM_avg%
  ...

--- end ---
```

## Dependencies

- A C++ compiler (g++ or clang++)
- FR_Math source (`../../src/FR_math.c`, `../../src/FR_math.h`, `../../src/FR_defs.h`)
- libfixmath source at `../../compare_lfm/libfixmath/libfixmath/`
- Standard C math library (`-lm`)
