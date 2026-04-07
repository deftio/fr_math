# FR_math.c Review Concerns and Test Recommendations

## Priority Summary

| Priority | Area | Concern | Why it matters | Recommended action |
|---|---|---|---|---|
| P0 | `FR_FixMulSat` | Saturation / overflow logic is explicitly marked as questionable in the code | This can silently return wrong values in core arithmetic paths | Rewrite around a proven overflow model, ideally with 64-bit intermediates when available, then fuzz and boundary test it |
| P0 | `FR_atan2` | Function name implies angle computation, but implementation returns only quadrant codes except on `x == 0` | Any caller expecting real `atan2` semantics will get incorrect results | Either implement real fixed-point `atan2` or rename to quadrant classifier :contentReference[oaicite:0]{index=0} |
| P0 | `FR_log2` / `FR_ln` / `FR_log10` | `FR_log2` appears incomplete and the dependent log functions inherit that | Log math will be untrustworthy across the library | Re-derive algorithm and build a reference test suite against double-precision golden outputs :contentReference[oaicite:1]{index=1} |
| P1 | `FR_Tan` | Stores `s32` results from `FR_TanI` into `s16` temporaries | High-angle values may truncate or wrap before interpolation | Change `i` and `j` to `s32` and add steep-angle tests around 89–91 degrees :contentReference[oaicite:2]{index=2} |
| P1 | `FR_acos` | Radix handling and edge-case logic look inconsistent after precision conversion | Risk of incorrect inverse trig outputs, especially near ±1 | Revisit algorithm assumptions and test full domain with tight error bounds :contentReference[oaicite:3]{index=3} |
| P1 | print helpers | Negating minimum signed integer can overflow | Formatting code may fail on edge values | Add safe min-int handling and explicit unsigned formatting paths :contentReference[oaicite:4]{index=4} |
| P2 | trig functions | `% 360` normalization is noted as expensive | Might matter on very small MCUs or hot loops | Consider faster range reduction if profiling shows it is hot :contentReference[oaicite:5]{index=5} |
| P2 | API clarity | Several functions require caller-managed radix alignment / interpretation | Easy to misuse even if implementation is correct | Tighten docs, add examples, and enforce assumptions in tests |

## Detailed Concern List

| ID | Function / Area | Concern | Risk | What to test | Fix recommendation |
|---|---|---|---|---|---|
| C1 | `FR_FixMulSat` | The code comment says the saturation check “might be a bug” and overflow detection relies on sign checks after intermediate arithmetic | Silent arithmetic corruption | Multiply combinations near `INT32_MAX`, `INT32_MIN`, `0`, `1`, `-1`, `0x7fff`, `0x8000`, and random large operands; compare to 64-bit reference saturated multiply | Replace with a mathematically proven implementation; prefer `int64_t` if target supports it :contentReference[oaicite:6]{index=6} |
| C2 | `FR_FixMulSat` | Intermediate partial sums may already overflow before saturation handling finishes | Undefined or compiler-dependent behavior | Compile with UB sanitizers on host; run randomized tests | Use unsigned carry-aware decomposition or 64-bit intermediate path |
| C3 | `FR_FixMuls` | Uses absolute value and sign reconstruction; may misbehave on minimum negative value depending on `FR_ABS` macro | Wrong sign / overflow on one edge | Test `INT32_MIN * 1`, `INT32_MIN * -1`, `INT32_MIN * 0` | Make min-negative handling explicit |
| C4 | `FR_FixAddSat` | Looks structurally okay, but needs confirmation at signed overflow boundaries | Core fixed-point add must be trustworthy | Test all overflow boundaries: `MAX+1`, `MAX+MAX`, `MIN-1`, `MIN+MIN`, mixed-sign adds | Keep if tests pass; document expected saturation behavior :contentReference[oaicite:7]{index=7} |
| C5 | `FR_CosI` / `FR_SinI` | Degree normalization and symmetry logic should be validated at all quadrant boundaries | Small off-by-one errors here poison many downstream functions | Test every integer degree from `-720` to `+720` against reference | Keep table approach if accurate |
| C6 | `FR_Cos` / `FR_Sin` | Interpolation across integer table points may have edge glitches around wrap and sign transitions | Visible precision discontinuities | Test around `-180`, `-90`, `0`, `90`, `180` with fractional radix inputs | Add max absolute error spec and verify |
| C7 | `FR_TanI` | Special handling for `90` and `270` is inconsistent with prior normalization to `[-180, 180]`; `270` path may never be reached after reduction | Dead or misleading logic | Test `90`, `-90`, `270`, `-270`, `450` | Normalize once, then handle only reachable canonical values :contentReference[oaicite:8]{index=8} |
| C8 | `FR_Tan` | `s32 FR_TanI(...)` assigned to `s16 i, j` truncates data before interpolation | Incorrect tangent near steep slopes | Test `80` to `89.9` degrees and negative equivalents | Change `i`, `j` to `s32` immediately :contentReference[oaicite:9]{index=9} |
| C9 | `FR_acos` | After converting input to `FR_TRIG_PREC`, code masks with `((1 << radix) - 1)` using caller radix, which looks suspicious | Domain reduction may be wrong | Sweep inputs from `-1.0` to `+1.0` at fine increments and compare against `acos()` | Re-check variable meanings after radix conversion :contentReference[oaicite:10]{index=10} |
| C10 | `FR_acos` | Special case for ±1 is commented with uncertainty in the source itself | Edge cases may be wrong | Test exact `-1`, `0`, `+1`, plus nearest representable neighbors | Replace with explicit domain checks and comments grounded in fixed-point encoding :contentReference[oaicite:11]{index=11} |
| C11 | `FR_asin` | Depends entirely on `FR_acos` correctness | Wrong inverse trig family results | Sweep domain and compare to reference | Validate only after `FR_acos` is fixed |
| C12 | `FR_atan2` | Returns quadrant code `0..3`, not an angle | Severe API contract mismatch | Test common points `(1,0)`, `(1,1)`, `(0,1)`, `(-1,1)`, `(-1,0)`, etc. | Implement real angle output or rename function to match behavior :contentReference[oaicite:12]{index=12} |
| C13 | `FR_pow2` | Nontrivial interpolation and correction logic needs accuracy characterization | Can drift significantly if coefficient logic is off | Sweep integer and fractional inputs across negative and positive ranges; compare to `pow(2,x)` | Keep only with quantified error bounds :contentReference[oaicite:13]{index=13} |
| C14 | `FR_pow2` | Shift operations on large `flr` values can overflow or become undefined | Dangerous at range extremes | Test very large positive and negative exponents | Add explicit clamp logic and validated range limits |
| C15 | `FR_log2` | Shifts input down but does not appear to accumulate log result | Likely not actually computing log2 | Test powers of two and nearby values: `1,2,4,8,16,3,5,6,7` | Re-implement based on leading-bit position plus fractional refinement :contentReference[oaicite:14]{index=14} |
| C16 | `FR_ln` / `FR_log10` | Built on `FR_log2`, so any flaw propagates | Broken transcendental results | Golden tests against `ln()` and `log10()` | Block release of these functions until `FR_log2` is validated :contentReference[oaicite:15]{index=15} |
| C17 | `FR_printNumF` | Negates signed integer part directly; min-negative can overflow | Edge-case formatting bug | Test `INT32_MIN`, `-1`, `0`, `1`, max positive values | Use unsigned magnitude conversion path for negatives :contentReference[oaicite:16]{index=16} |
| C18 | `FR_printNumF` | Fraction formatting logic should be checked for leading zeros and rounding behavior | User-visible formatting glitches | Test values like `0.0001`, `0.5`, `1.05`, `-1.05`, and varying `prec` | Decide whether truncation or rounding is desired; document it |
| C19 | `FR_printNumD` | Same min-negative overflow concern as above | Edge-case formatting bug | Test `INT_MIN`, `-1`, `0`, `INT_MAX` | Same unsigned-magnitude strategy :contentReference[oaicite:17]{index=17} |
| C20 | `FR_printNumH` | Right-shifting signed `int` is implementation-defined for negatives | Cross-compiler inconsistency | Test negative hex formatting on multiple compilers / targets | Cast to unsigned before shifting :contentReference[oaicite:18]{index=18} |
| C21 | whole file | Several comments indicate uncertainty or TODO-level confidence in math paths | Signals unfinished code mixed with production code | Review every function for “experimental vs stable” status | Mark unstable APIs clearly or move them behind feature flags :contentReference[oaicite:19]{index=19} |

## Test Plan Recommendations

### 1. Golden-reference tests
Use host-side `double` math as the oracle for:
- multiply saturation
- trig
- inverse trig
- `pow2`
- `log2`
- `ln`
- `log10`

Store acceptable error thresholds per function, not one blanket epsilon.

### 2. Boundary tests
For every arithmetic function, test:
- `0`
- `1`
- `-1`
- maximum positive
- minimum negative
- just-below / just-above saturation thresholds
- radix values `0`, `1`, common production radices, and largest supported radix

### 3. Domain sweep tests
For trig and inverse trig:
- sweep full angle ranges
- focus on discontinuity points: `-180`, `-90`, `0`, `90`, `180`
- focus on inverse trig domain edges: `-1`, `+1`

### 4. Property tests
Useful invariants:
- `sin(-x) = -sin(x)`
- `cos(-x) = cos(x)`
- `asin(x)` should be monotonic on `[-1,1]`
- `acos(x)` should be monotonic decreasing on `[-1,1]`
- `log2(2^x) ~= x` where both functions are valid
- saturated multiply result should never wrap sign unexpectedly

### 5. Cross-compiler tests
Run the same suite on:
- GCC
- Clang
- target embedded compiler if different

This matters because signed overflow and right-shift behavior can vary in ugly ways.

### 6. Fuzz / randomized tests
For arithmetic-heavy functions:
- generate random operands
- compare to a higher-precision reference implementation
- keep failing seeds for regression

## Suggested Fix Order

1. Lock down `FR_FixMulSat`
2. Fix or remove `FR_atan2`
3. Fix `FR_log2`, then `FR_ln` and `FR_log10`
4. Fix `FR_Tan` truncation
5. Revisit `FR_acos` / `FR_asin`
6. Harden print helpers
7. Add performance review only after correctness is proven

## Release Gate Suggestion

Do not treat this as a production-safe math library until:
- all P0 items are fixed or explicitly disabled
- boundary tests pass
- golden-reference comparison exists for all nonlinear math
- API names match actual behavior