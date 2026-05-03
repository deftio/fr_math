Running benchmarks (65536 accuracy pts, 100000 timing iters)...
  sin done
  cos done
  tan done
  asin done
  acos done
  atan done
  atan2 done
  sqrt done
  exp done
  ln done
  log2 done
  mul done
  div done
  hypot done
  hypot_fast8 done

## FR_math vs libfixmath — Q16.16 comparison

All errors measured vs IEEE 754 double. Pct errors skip |ref| < 0.01.

### Accuracy

| Function | FR max LSB | FR max %% | FR avg %% | lfm max LSB | lfm max %% | lfm avg %% | Winner |
|----------|----------:|---------:|---------:|----------:|---------:|---------:|--------|
| sin             |     4.9 |  0.4816 |  0.0081 |     507.6 | 74.5513 |  0.6105 | FR       |
| cos             |     4.4 |  0.3282 |  0.0077 |     508.3 | 74.4001 |  0.6121 | FR       |
| tan             |    13.0 |  0.1551 |  0.0055 |    1196.0 |  0.7099 |  0.0410 | FR       |
| asin            |    24.9 |  1.9776 |  0.0477 |     667.1 | 20.1233 |  2.4452 | FR       |
| acos            |    24.6 |  0.2724 |  0.0093 |     667.8 | 15.3142 |  0.3475 | FR       |
| atan            |    59.9 |  0.2149 |  0.0061 |     666.3 | 19.8632 |  0.4571 | FR       |
| atan2           |    62.5 |  0.4122 |  0.0239 |     666.7 | 20.0045 |  0.9267 | FR       |
| sqrt            |     0.5 |  0.0062 |  0.0001 |       0.5 |  0.0062 |  0.0001 | tie      |
| exp             |   208.3 |  0.1486 |  0.0078 |     216.3 |  0.0756 |  0.0042 | FR       |
| ln              |     3.2 |  0.3012 |  0.0006 |       2.2 |  0.0557 |  0.0002 | lfm      |
| log2            |     4.0 |  0.4945 |  0.0006 |       2.3 |  0.1758 |  0.0002 | lfm      |
| mul             |     0.5 |  0.0692 |  0.0004 |       0.5 |  0.0692 |  0.0004 | tie      |
| div             |     0.5 |  0.0727 |  0.0010 |       0.5 |  0.0727 |  0.0010 | FR       |
| hypot           |     0.5 |  0.0076 |  0.0009 |       --- |      --- |      --- | FR only  |
| hypot_fast8     | 89944.4 |  0.1372 |  0.0516 |       --- |      --- |      --- | FR only  |

### Speed (ns/call, lower is better)

| Function | FR_math | libfixmath | Speedup | Faster |
|----------|--------:|-----------:|--------:|--------|
| sin             |    5.6 |       10.6 |   1.91x | FR      |
| cos             |    8.9 |       13.3 |   1.50x | FR      |
| tan             |    7.1 |       32.6 |   4.57x | FR      |
| asin            |    9.7 |       49.5 |   5.11x | FR      |
| acos            |    8.4 |       50.7 |   6.03x | FR      |
| atan            |    8.1 |       11.0 |   1.37x | FR      |
| atan2           |   15.9 |       10.9 |   0.69x | lfm     |
| sqrt            |   18.6 |       19.9 |   1.07x | FR      |
| exp             |    3.0 |       64.7 |  21.28x | FR      |
| ln              |    9.0 |      453.2 |  50.53x | FR      |
| log2            |    8.5 |       39.4 |   4.63x | FR      |
| mul             |    0.9 |        1.2 |   1.33x | FR      |
| div             |    0.9 |        5.3 |   6.10x | FR      |
| hypot           |   19.9 |        --- |     --- | FR only |
| hypot_fast8     |    2.6 |        --- |     --- | FR only |

### Summary (13 head-to-head functions)

- **Speed**: FR_math 12 / 13, libfixmath 1 / 13
- **Accuracy**: FR_math 9 / 13, libfixmath 2 / 13, tie 2 / 13
- Accuracy = 65536-pt sweep at Q16.16; timing = min of 3 x 100k calls

### Compiled size (clang -O2, macOS ARM)

| | FR_math | libfixmath | lfm (no cache) |
|---|---:|---:|---:|
| Code (text) | 6,652 B | 4,880 B | 5,444 B |
| Tables (ROM) | 818 B | 32 B | 32 B |
| **ROM total** | **7,470 B** | **4,912 B** | **5,476 B** |
| BSS / RAM | **0 B** | **112 KB** | **0 B** |

FR_math packs trig, inv-trig, log/ln/log10, exp/pow2/pow10, sqrt, hypot(2),
waves(6), ADSR, print into 7.3 KB ROM with zero RAM overhead.
libfixmath (trig, inv-trig, log/log2, exp, sqrt, mul/div, str) is 4.8 KB ROM
but caches 112 KB of sin/exp LUTs in BSS at runtime.

