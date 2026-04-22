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
| sin             |     8.8 |  1.0615 |  0.0158 |     507.6 | 74.5513 |  0.6105 | FR       |
| cos             |     8.2 |  0.9018 |  0.0161 |     508.3 | 74.4001 |  0.6121 | FR       |
| tan             |    55.7 |  1.0080 |  0.0228 |    1196.0 |  0.7099 |  0.0410 | FR       |
| asin            |    31.3 |  0.5795 |  0.0134 |     667.1 | 20.1233 |  2.4452 | FR       |
| acos            |    31.0 |  0.5194 |  0.0056 |     667.8 | 15.3142 |  0.3475 | FR       |
| atan            |    62.7 |  0.2149 |  0.0061 |     666.3 | 19.8632 |  0.4571 | FR       |
| atan2           |    63.6 |  0.4122 |  0.0258 |     666.7 | 20.0045 |  0.9267 | FR       |
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
| sin             |    2.6 |       20.7 |   7.94x | FR      |
| cos             |    4.8 |       18.4 |   3.86x | FR      |
| tan             |    6.0 |       41.4 |   6.89x | FR      |
| asin            |   11.5 |       53.7 |   4.67x | FR      |
| acos            |    8.4 |       50.4 |   5.97x | FR      |
| atan            |    8.0 |       11.2 |   1.41x | FR      |
| atan2           |   15.9 |       10.5 |   0.66x | lfm     |
| sqrt            |   18.6 |       19.8 |   1.06x | FR      |
| exp             |    3.1 |       67.6 |  22.02x | FR      |
| ln              |    8.8 |      479.3 |  54.70x | FR      |
| log2            |    8.7 |       39.4 |   4.55x | FR      |
| mul             |    0.9 |        1.2 |   1.33x | FR      |
| div             |    0.9 |        5.2 |   5.98x | FR      |
| hypot           |   20.0 |        --- |     --- | FR only |
| hypot_fast8     |    2.4 |        --- |     --- | FR only |

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

