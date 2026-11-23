Running benchmarks (every input per domain)...
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

## FR_math vs libfixmath — Q16.16 comparison

Every Q16.16 input in each domain (2-input functions: every pair on the stated grid),
vs IEEE 754 double. Nothing is skipped. Error = |got - exact| / max(|exact|, 1.0);
beyond the Q16.16 range (tan at its poles) a saturated result of the right sign counts as exact.

### Accuracy

| Function | Inputs | Sweep | FR max % | FR avg % | lfm max % | lfm avg % | More accurate |
|----------|-------:|-------|---------:|---------:|----------:|----------:|---------------|
| sin | 823549 | every input in [-2pi, 2pi] | 0.0076 | 0.0020 | 0.7758 | 0.0536 | FR |
| cos | 823549 | every input in [-2pi, 2pi] | 0.0082 | 0.0020 | 0.7769 | 0.0537 | FR |
| tan | 823549 | every input in [-2pi, 2pi], poles included | 0.0439 | 0.0048 | 100.0000 | 0.7690 | FR |
| asin | 131073 | every input in [-1, 1] | 0.0298 | 0.0103 | 1.0179 | 0.3536 | FR |
| acos | 131073 | every input in [-1, 1] | 0.0433 | 0.0070 | 1.0178 | 0.2421 | FR |
| atan | 8388609 | every input in [-64, 64] | 0.0963 | 0.0045 | 1.0176 | 0.3703 | FR |
| atan2 | 3145728 | every (y, x) in [-512, 511]^2 LSB, x 2^0, 2^8, 2^14 | 0.0976 | 0.0144 | 1.0176 | 0.2305 | FR |
| sqrt | 67108865 | every input in [0, 1024] | 0.0008 | 0.0000 | 0.0008 | 0.0000 | tie |
| exp | 1729954 | every input in [-16, 10.397] | 0.0025 | 0.0006 | 135.7910 | 0.0020 | FR |
| ln | 67108864 | every input in (0, 1024] | 0.0049 | 0.0003 | 0.0665 | 0.0001 | FR |
| log2 | 67108864 | every input in (0, 1024] | 0.0061 | 0.0003 | 100.0000 | 0.0001 | FR |
| mul | 4194304 | 2048x2048 grid, a in [-50,50], b in [-2,2] | 0.0008 | 0.0001 | 0.0008 | 0.0001 | tie |
| div | 4194304 | 2048x2048 grid, a in [-50,50], b in [0.5,50] | 0.0008 | 0.0003 | 0.0009 | 0.0003 | FR |
| hypot | 3145728 | every (x, y) in [-512, 511]^2 LSB, x 2^0, 2^8, 2^14 | 0.0008 | 0.0002 | --- | --- | FR only |
| hypot_fast8 | 3145728 | every (x, y) in [-512, 511]^2 LSB, x 2^0, 2^8, 2^14 | 0.1372 | 0.0330 | --- | --- | FR only |

### Speed (mean ns/call over the input set, lower is better)

| Function | FR_math | libfixmath | Speedup | Faster |
|----------|--------:|-----------:|--------:|--------|
| sin | 3.1 | 6.2 | 2.02x | FR |
| cos | 3.9 | 6.4 | 1.63x | FR |
| tan | 4.4 | 17.9 | 4.04x | FR |
| asin | 5.1 | 29.6 | 5.76x | FR |
| acos | 5.2 | 30.3 | 5.82x | FR |
| atan | 5.3 | 7.7 | 1.47x | FR |
| atan2 | 10.8 | 6.5 | 0.60x | lfm |
| sqrt | 13.6 | 13.1 | 0.97x | lfm |
| exp | 2.3 | 64.1 | 27.81x | FR |
| ln | 8.0 | 45.1 | 5.67x | FR |
| log2 | 8.0 | 26.0 | 3.25x | FR |
| mul | 1.5 | 1.6 | 1.03x | FR |
| div | 1.5 | 4.1 | 2.66x | FR |
| hypot | 12.0 | --- | --- | FR only |
| hypot_fast8 | 2.4 | --- | --- | FR only |

### Summary (13 head-to-head functions)

- **Speed**: FR_math 11 / 13, libfixmath 2 / 13
- **Accuracy (max error)**: FR_math 11 / 13, libfixmath 0 / 13, tie 2 / 13

Worst-case inputs (FR / lfm):

- sin: -3.9210968 / -3.1419983
- cos: -1.7299042 / -4.7127838
- tan: -1.7486877 / -4.7124176
- asin: 0.99438477 / -0.11584473
- acos: 0.99438477 / 0.99259949
- atan: -1.4934998 / -0.12072754
- atan2: -1.9609375 / -0.00085449219
- sqrt: 0.99998474 / 0.99998474
- exp: 1.4675598 / -11.090363
- ln: 1.0398865 / 1.5258789e-05
- log2: 1.0393372 / 3.0517578e-05
- mul: -22.007812 / -22.007812
- div: -49.218369 / -2.3204651
- hypot: -1
- hypot_fast8: -120.25

Compiled size: run `make size` in this directory.
