# FR_Math 2.0 — Implementation Plan and Roadmap

> Living document. Check items off as they land. Each phase has acceptance criteria.
> Keep this file updated as we make decisions or discover new constraints.

## North Star

FR_Math 2.0 is a backwards-incompatible cleanup of the fixed-point math library that:

1. **Fixes every known correctness bug** identified by the TDD characterization suite
   (`tests/test_tdd.cpp`, report at `build/test_tdd_report.md`).
2. **Migrates trig from degree-native to radian-native**, with three coordinated APIs
   (degrees / radians / BAM) sharing a single internal table.
3. **Provides a clean legacy compatibility shim** (`FR_legacy.h`) so existing v1
   callers can opt-in to the old names without polluting the new headers.
4. **Documents accuracy and edge cases** in both source and README.
5. **Lands all of the above with characterization tests staying green** (the suite
   measures behavior; we update the report each step and watch for regressions).

The TDD characterization suite is the spec. If a function changes behavior, the
report will show it, and we will explicitly bless or revert the change.

---

## Phase Order (rationale)

```
Phase 0: Foundations and macro audit  ← do not skip
Phase 1: Bug fixes (atan2 first, then the rest)
Phase 2: Radian-native trig + BAM tables
Phase 3: Radian inverse trig
Phase 4: 2D matrix updates and new convenience APIs
Phase 5: Legacy compatibility shim
Phase 6: Documentation, README, examples
Phase 7: Release prep, version bump, CI matrix
```

Phase 1 must finish before phase 2 starts. We do not want to bake the new API on
top of broken arithmetic. Once phase 1 is done, phases 2–4 can proceed in order.
Phases 5–7 follow at the end.

---

## Phase 0 — Foundations and macro audit

**Goal:** establish the ground truth before changing anything. No production-code
changes in this phase. Build the inventory we'll work from.

### Tasks

- [ ] **0.1** — Generate a complete list of every `#define FR_…(…)` in `FR_math.h`
      and `FR_defs.h` and check it against the table-of-contents in section 2 / 3
      of `tests/test_tdd.cpp`. Add a row to a new section "2.0 Macro Inventory"
      for any macro currently missing.
- [ ] **0.2** — For each macro, characterize:
  - argument count
  - whether it evaluates each argument more than once (side-effect safety)
  - whether it widens or narrows
  - the documented vs. observed type behavior on signed-negative inputs
  - documented vs. observed worst-case relative error (for approximation macros)
- [ ] **0.3** — Add `tools/audit_macros.py` that parses `FR_math.h` for `#define`
      lines and prints a checklist diff against the test sections. Run it from
      `make test-tdd` so we can't silently lose coverage.
- [ ] **0.4** — Confirm every macro in `FR_math.h` has at least one row in the
      report. Update `test_tdd.cpp` for any gaps.
- [ ] **0.5** — Add an explicit "max safe input" probe for every shift-based
      approximation macro that uses leading `<<` (e.g., `FR_DEG2RAD`,
      `FR_SMUL10`, `FR_SLOG2_10`). Document the value where the leading shift
      first overflows.
- [ ] **0.6** — Document the LP64 caveat at the top of the test report (already
      partially done) and tag every test that depends on 32-bit semantics.
- [ ] **0.7** — Decide: do we typedef `s32` to `int32_t` from `<stdint.h>` now,
      or as part of Phase 1.10? **Recommendation:** do it now in Phase 0 because
      every other phase depends on it for honest test results.

### Acceptance criteria

- Every macro in `FR_math.h` and `FR_defs.h` is referenced at least once in
  `test_tdd.cpp`, verified by `tools/audit_macros.py`.
- `s32` is `int32_t` and `tests/test_tdd.cpp` section 0 reports `s32 = 4 bytes`.
- Report regenerates cleanly with no LP64 warning.

---

## Phase 1 — Bug fixes

**Goal:** every function the report flagged as broken in v1 is fixed in place,
without changing any function names or signatures yet. Each fix is its own
commit with a test sweep added or strengthened to lock the behavior.

Order matters: `FR_atan2` first (most contract-broken, blocks trig migration),
then arithmetic primitives, then transcendentals, then print helpers.

### 1.1 — Rewrite `FR_atan2` as a real `atan2`

**Why first:** the function name and signature promise an angle, the body
returns 0..3. Nothing currently depends on the broken behavior because the
broken behavior is meaningless. This is the lowest-risk, highest-impact fix.

**Why div/0 matters:** users currently can't call `FR_atan(y/x)` for `x==0`.
A correct `FR_atan2(y, x)` removes that branch from every caller because
`atan2` handles all four quadrants and the axes natively.

- [ ] **1.1.a** — Decide on output unit. **Recommendation:** keep v1
      `FR_atan2` returning *degrees* in `s16` (so the v1 signature stays
      compatible with anyone who has been calling it expecting some kind of
      angle); add `fr_atan2(y, x)` returning *radians* as the v2 entry point in
      Phase 3.
- [ ] **1.1.b** — Implement using either CORDIC or table-based octant reduction
      + `FR_atan` table. CORDIC is the smallest-flash option; octant + table
      is faster. **Recommendation:** octant reduction down to `[0, π/4]` then
      use a small (33-entry) `arctan` table with linear interpolation. Stays
      consistent with the rest of the library's table-based style.
- [ ] **1.1.c** — Handle all axis cases explicitly (`x==0 && y>0`, `x==0 && y<0`,
      `x==0 && y==0`, `x!=0 && y==0`).
- [ ] **1.1.d** — Add `FR_atan2` to `FR_math.h` (currently it's in the .c
      file only). This is a header bug independent of the body bug.
- [ ] **1.1.e** — Strengthen `tests/test_tdd.cpp` section 7.3 to sweep `(y, x)`
      around the unit circle in 5° increments and compare against `atan2()`.
      Tighten the report from "KNOWN BROKEN" to "max abs err = N degrees".

**Acceptance:** report section 7.3 shows max abs error < 1° over the full sweep.

### 1.2 — Fix `FR_log2`

The current body has a bit-position counter `h` that's used as a shift width
but never accumulated into the result. The function returns the *reduced
input*, not log2 of the input.

- [ ] **1.2.a** — Replace with leading-bit-position + fractional refinement.
      Algorithm: find the index of the leading 1 bit (`floor(log2(x))`), then
      use a small lookup table indexed by the next 5–7 bits to refine the
      fractional part.
- [ ] **1.2.b** — Generate the refinement table via `tools/coef-gen.py` so
      it's reproducible.
- [ ] **1.2.c** — Add full sweep test in `test_tdd.cpp` section 8.3:
      `for x in [1, 1<<24] step 1.01: |FR_log2(x) - log2(x)|`.

**Acceptance:** section 8.3 shows max abs err < 0.01 across the full sweep.

### 1.3 — Fix `FR_ln` and `FR_log10`

These are one-liners on top of `FR_log2`. Once `FR_log2` is fixed, these
inherit correctness, but we should confirm they use the
shift-macro multipliers (`FR_SrLOG2E`, `FR_SrLOG2_10`) correctly.

- [ ] **1.3.a** — Verify `FR_SrLOG2E` is applied at the correct radix.
- [ ] **1.3.b** — Verify `FR_SrLOG2_10` likewise.
- [ ] **1.3.c** — Update test sections 8.4 and 8.5 to be sweeps with stats,
      not just spot checks.

**Acceptance:** sections 8.4 / 8.5 show max abs err < 0.02 across full sweep.

### 1.4 — Fix `FR_pow2` negative-fractional path

Current bug: `FR_pow2(-0.5)` returns ~1.41, not 0.71. The negative-`flr`
branch is wrong.

- [ ] **1.4.a** — Trace through the negative branch with paper math and
      identify the off-by-one or wrong-shift.
- [ ] **1.4.b** — The likely fix is that the fractional refinement is being
      applied with the wrong sign or before the integer shift, not after.
- [ ] **1.4.c** — Verify `FR_pow2(x)` and `1.0 / FR_pow2(-x)` agree.
- [ ] **1.4.d** — Replace fine sweep test in section 8.2 with stricter
      tolerance after the fix.

**Acceptance:** section 8.2 shows max abs err < 1e-3 across `[-8, 8] step 0.01`.

### 1.5 — Fix `FR_FixMulSat`

Current bug: the formula `(h<<16) + m1 + m2 + l` adds the *full* low term
`l = (x&0xffff)*(y&0xffff)` instead of `l>>16`. This means small inputs
return values shifted 16 bits too high (e.g., `1*1` returns 1 instead of 0).
The "TODO: might be a bug" comment in the source is honest.

- [ ] **1.5.a** — On targets where `int64_t` is available (everywhere modern),
      use the obvious correct path:
      `int64_t v = (int64_t)x * (int64_t)y >> 16; saturate; return`.
      Add a `#if defined(FR_NO_INT64)` fallback for the original intent.
- [ ] **1.5.b** — Confirm `FR_FixMuls` and `FR_FixMulSat` agree on
      non-saturating inputs. The current versions disagree.
- [ ] **1.5.c** — Add a million-pair fuzz test (random `s32` pairs) to
      section 4.2.
- [ ] **1.5.d** — Verify saturation cases match `sat32((x*y)>>16)` reference.

**Acceptance:** section 4.2 shows `differs from sat(|x|*|y|>>16) ref: 0`
across the full sweep.

### 1.6 — Fix `FR_Tan` (s16 truncation)

Current bug: `s32 i, j` should be the type of the locals receiving
`FR_TanI()` results, but they're declared as `s16`. Steep angles wrap
silently.

- [ ] **1.6.a** — Change `s16 i, j` to `s32 i, j` in `FR_Tan`.
- [ ] **1.6.b** — Audit other functions for the same pattern.
- [ ] **1.6.c** — Re-run section 6.2 and confirm `tan(60°)` returns ~1.73.

**Acceptance:** section 6.2 shows max abs err < 0.01 for all angles up to 89°.

### 1.7 — Fix `FR_DEG2RAD` / `FR_RAD2DEG` swap and parens

- [ ] **1.7.a** — Swap the macro bodies to match their names. Verify against
      section 3 / 3.1 of the report.
- [ ] **1.7.b** — Add the missing parens around `x` in the
      `(x >> 4)` subexpression in `FR_DEG2RAD`.
- [ ] **1.7.c** — Add a section 3 row that proves `FR_DEG2RAD(180) ≈ π`.
- [ ] **1.7.d** — Document the worst-case relative error of each macro
      *next to its definition* in the header.

**Acceptance:** section 3 shows `FR_DEG2RAD(180)` ≈ 3.1416 (in whatever
radix), and report's "**MISMATCH**" rows for these two macros become "OK".

### 1.8 — Fix `FR_NUM`

Currently ignores its `radix` argument. Either fix the body or remove the
macro.

- [ ] **1.8.a** — Decide: fix or remove? **Recommendation:** fix. Many users
      may expect this macro from the name. Removing is a silent breakage.
- [ ] **1.8.b** — If fixing, the body should be
      `((s32)(x) << (radix)) | ((s32)(frac) & ((1L << (radix)) - 1))` or
      similar — confirm against the original intent in the comment.
- [ ] **1.8.c** — Section 2.4 of the report should show `FR_NUM(12, 2470, 8)`
      ≈ `12.0 + 2470/2^16` (or whatever the corrected semantics are).

**Acceptance:** section 2.4 shows the expected radix-aware result.

### 1.9 — Fix `FR_SQUARE`

Current body has a missing close paren. Uncompilable. Not currently used by
anything.

- [ ] **1.9.a** — Add the missing paren: `((FR_FIXMUL32u((x), (x))))`.
- [ ] **1.9.b** — Add a section 2.18 row that actually compiles and exercises
      the macro.
- [ ] **1.9.c** — Confirm `FR_SQUARE(I2FR(2, 16))` ≈ `I2FR(4, 16)`.

**Acceptance:** macro compiles and produces correct values for both signs.

### 1.10 — Fix `FR_printNumD` (and friends)

Current bugs:
- `n = -n` overflows for `INT_MIN` (mangles output).
- Returns `FR_S_OK` (= 0) instead of bytes-written, despite the `int` return.

- [ ] **1.10.a** — Use unsigned magnitude for the integer part:
      `unsigned int u = (n < 0) ? (unsigned int)(-(n + 1)) + 1 : (unsigned int)n;`
- [ ] **1.10.b** — Track and return the byte count.
- [ ] **1.10.c** — Decide: change the return type? **Recommendation:** keep
      `int`, just return the count. `FR_E_FAIL` is `1` so 1+ legitimate
      counts are indistinguishable from a failure. Use `-1` for failure.
- [ ] **1.10.d** — Update header docstring and section 9.1 of the report.

**Acceptance:** section 9.1 shows correct output for `INT_MAX`, `INT_MIN`,
and the byte count column matches the visible string length.

### 1.11 — Fix `FR_printNumF`

Current bugs:
- Same `INT_MIN` overflow as 1.10.
- Fraction logic produces wrong digits for small fractions
  (`0.0001` → "0.9000", `1.05` → "1.4998"). This is a separate bug from
  the integer-part overflow.

- [ ] **1.11.a** — Apply 1.10's unsigned-magnitude fix to the integer part.
- [ ] **1.11.b** — Re-derive the fraction extraction. The current code looks
      like it's converting the binary fraction to decimal by repeated `*10`
      then `>>radix`, which is correct in principle but is producing wrong
      digit boundaries somewhere.
- [ ] **1.11.c** — Decide rounding: round-half-even vs. truncate. Document.
- [ ] **1.11.d** — Section 9.3 should produce sensible output for all rows.

**Acceptance:** `FR_printNumF` round-trips through `atof` to within 1 LSB
for all section 9.3 cases.

### 1.12 — Fix `FR_printNumH`

The current code is right-shifting a signed int, which is implementation-
defined for negatives. Cast to unsigned first.

- [ ] **1.12.a** — Cast `n` to `unsigned int` before shifting.
- [ ] **1.12.b** — Confirm output matches on GCC, Clang, and at least one
      cross-compiler (e.g., `arm-none-eabi-gcc`).

**Acceptance:** section 9.2 produces identical output on all toolchains
in CI.

### 1.13 — Fix `FR_atan` (missing implementation)

`FR_atan` is declared in `FR_math.h` line 292 but not defined anywhere.
Calling it is a link error.

- [ ] **1.13.a** — Decide: implement or remove? **Recommendation:** implement
      as a thin wrapper over the new `FR_atan2(y, 1)`.
- [ ] **1.13.b** — Add to test section 7 with a sweep.

**Acceptance:** `FR_atan(I2FR(1, 16), 16)` ≈ 45° (or π/4 in v2).

### 1.14 — Remove `FR_TanI` dead code

The `if (270 == deg)` branch in `FR_TanI` is unreachable after the prior
`% 360` then `[-180, 180]` reduction. gcov confirms 0 hits.

- [ ] **1.14.a** — Delete the dead branch.
- [ ] **1.14.b** — Verify gcov still reports the same coverage on
      reachable lines.

**Acceptance:** `FR_math.c` line count drops by ~2; gcov reports 100% on
the function.

### 1.15 — Audit `FR_FixAddSat`

It looks structurally correct, but on the LP64 host the test is using
64-bit accumulators, which masks any 32-bit issue. With `s32` now
`int32_t` (Phase 0.7), re-run the test and confirm it's still clean.

- [ ] **1.15.a** — Re-run section 4.4 with `s32 = int32_t`.
- [ ] **1.15.b** — If it fails on negative+positive overflow boundaries,
      fix and add specific test cases.

**Acceptance:** section 4.4 shows `differs: 0` after the typedef change.

### Phase 1 acceptance criteria (overall)

- Every "BROKEN" cell in section 11.1 of the report becomes "OK" or
  is explicitly documented as a v1 → v2 behavior change.
- `make test-tdd && diff -q build/test_tdd_report.md dev/baseline_report.md`
  shows the expected diffs only.
- `make test` (the existing assert-based suites) still passes after each
  fix; no v1 callsite is silently broken.
- Tag the tip of phase 1 as `v1.5.0-prefix` (a "v1 with bugs fixed but
  with the v1 API") so anyone who wants the bugs fixed without the API
  rewrite can build that tag.

---

## Phase 2 — Radian-native trig + BAM tables

**Goal:** introduce `fr_sin` / `fr_cos` / `fr_tan` as the new primary trig
functions, taking radians as input, with internal BAM-based table lookup.
Old `FR_Sin` / `FR_Cos` / `FR_Tan` (degree-based) remain in the file but
become wrappers over the new primitives in Phase 5.

### Tasks

- [ ] **2.1** — Add `tools/coef-gen.py` invocation to produce a 129-entry
      cosine table covering `[0, π/2]` inclusive in s0.15 format.
      Output: `src/FR_trig_table.h` (auto-generated, header guard, no
      hand-edits — script-regenerated).
- [ ] **2.2** — Add the BAM macros to `FR_math.h`:
  ```
  /* full circle = 2^16 BAM units. Quadrant = 2^14. */
  #define FR_BAM_BITS         (16)
  #define FR_BAM_FULL         (1 << FR_BAM_BITS)
  #define FR_BAM_QUADRANT     (FR_BAM_FULL >> 2)
  #define FR_BAM_TABLE_BITS   (7)         /* 128 entries */
  #define FR_BAM_INTERP_BITS  (FR_BAM_BITS - 2 - FR_BAM_TABLE_BITS)
  #define FR_RAD2BAM(rad, radix)  /* multiply by 2^16 / (2π) at given radix */
  #define FR_BAM2RAD(bam, radix)  /* inverse */
  #define FR_DEG2BAM(deg)         /* deg * (2^16 / 360) — exact via shift+add */
  #define FR_BAM2DEG(bam)         /* inverse */
  ```
- [ ] **2.3** — Implement `fr_sin_bam(u16 bam)` and `fr_cos_bam(u16 bam)`:
  - Mask quadrant from top 2 bits.
  - Index into 128-entry table from next 7 bits.
  - Use remaining 7 bits for linear interpolation.
  - Apply quadrant sign / mirror.
  - Return s0.15.
- [ ] **2.4** — Implement `fr_sin(s32 rad, u16 radix)` and `fr_cos(...)` as
      one-liner wrappers: `return fr_sin_bam(FR_RAD2BAM(rad, radix));`.
- [ ] **2.5** — Implement `fr_tan(s32 rad, u16 radix)` as
      `(fr_sin << k) / fr_cos` at the requested output radix, with explicit
      saturation for `cos == 0` cases.
- [ ] **2.6** — Add `fr_sin_deg(s16 deg)` etc. wrappers using
      `FR_DEG2BAM(deg)` for callers who want a single-line degree call but
      with the new sign / range conventions.
- [ ] **2.7** — Add a section 12 to `test_tdd.cpp` that:
  - Sweeps `fr_sin_bam` over all 65536 BAM values and compares to libm
    `sin`. Stats table.
  - Sweeps `fr_sin(rad, 16)` over `rad` in `[-2π, 2π]` step 0.001.
  - Cross-checks `fr_sin(FR_DEG2RAD(d), 16) == fr_sin_deg(d)` for
    integer degrees in `[-720, 720]`.
  - Cross-checks `fr_sin_deg(d) == FR_SinI(d)` for integer degrees
    (verifies the new path matches the old integer path bit-for-bit
    at integer degrees).
- [ ] **2.8** — Confirm `fr_sin_bam` is bit-exact at quadrant edges
      (`bam = 0`, `BAM_QUADRANT`, `2*BAM_QUADRANT`, `3*BAM_QUADRANT`).

### Acceptance

- Section 12 max abs err < 4e-5 (1.3 LSB in s0.15) over the full BAM
  sweep, < 5e-5 over the radian sweep.
- `fr_sin_deg(d) == FR_SinI(d)` for every integer `d` in `[-720, 720]`.
- gcov shows `fr_sin_bam` 100% covered.

---

## Phase 3 — Radian inverse trig

**Goal:** `fr_asin`, `fr_acos`, `fr_atan2` return *radians*, not degrees.
The v1 versions stay alongside (returning degrees) until Phase 5 moves
them behind the legacy header.

### Tasks

- [ ] **3.1** — Implement `fr_atan2(s32 y, s32 x, u16 radix)` returning
      radians at `radix`. Use the same octant-reduction approach as
      Phase 1.1, but return `radians` directly (no deg→rad conversion).
- [ ] **3.2** — Implement `fr_asin(s32 x, u16 radix)` and
      `fr_acos(s32 x, u16 radix)`. **Recommendation:** derive from
      `fr_atan2`: `fr_asin(x) = fr_atan2(x, sqrt(1 - x²))`.
- [ ] **3.3** — Need `fr_sqrt` for `fr_asin` / `fr_acos` derivation.
      Add `fr_sqrt(s32 x, u16 radix)` using Newton's method or a
      bit-by-bit algorithm. **Recommendation:** bit-by-bit; converges
      in fixed iterations, no division.
- [ ] **3.4** — Add convenience macros: `FR_RAD2DEG_FX(rad, radix)` and
      `FR_DEG2RAD_FX(deg, radix)` for callers who want to keep using
      degrees at their UI layer but call radian primitives internally.
- [ ] **3.5** — Update test sections 7.1 / 7.2 / 7.3 to use the new
      radian primitives. Tighten tolerances now that the math is correct.

### Acceptance

- `fr_acos`, `fr_asin`: max abs err < 1e-3 rad over `[-1, 1]`.
- `fr_atan2`: max abs err < 1e-3 rad over the full unit circle in
  5° increments.

---

## Phase 4 — 2D matrix updates

**Goal:** `FR_Matrix2D_CPT` learns to take radians via a new method
without changing the existing degree-based ones.

### Tasks

- [ ] **4.1** — Add `setrotate_rad(s32 rad, u16 radix)` alongside
      `setrotate(s16 deg)`. Both use the same internal table — the new
      one calls `fr_sin` / `fr_cos`, the old one calls `FR_SinI` /
      `FR_CosI`.
- [ ] **4.2** — Confirm via test that `setrotate(45) ≡ setrotate_rad(π/4)`
      to within 1 LSB.
- [ ] **4.3** — No other matrix changes needed; the existing class is
      correct per section 10.

### Acceptance

- New section 10.11 in the report sweeps both rotation entry points and
  confirms agreement to 1 LSB.

---

## Phase 5 — Legacy compatibility shim

**Goal:** existing v1 callers can `#include "FR_legacy.h"` to get the
old names mapped to the new functions. New users never see these.

### Tasks

- [ ] **5.1** — Create `src/FR_legacy.h`. It does *not* declare the v1
      functions itself — it provides macro aliases:
      ```
      #define FR_Sin(deg, radix)  fr_sin_deg(deg)   /* v1 took degrees, v2 too */
      #define FR_Cos(deg, radix)  fr_cos_deg(deg)
      #define FR_atan2(y, x, r)   fr_atan2_deg(y, x, r)  /* v2 returns degrees here */
      …
      ```
- [ ] **5.2** — Decide which v1 names are aliasable directly (degree-in,
      degree-out) and which need adapter functions (because the v1 sign
      convention differs). **Recommendation:** anything that can be a
      macro is a macro.
- [ ] **5.3** — Move every v1 function declaration that's been replaced
      out of `FR_math.h` and into `FR_legacy.h`.
- [ ] **5.4** — Add a CI build that compiles a "legacy mode" example
      using only `FR_legacy.h` includes, to prove the shim still
      works.

### Acceptance

- A new `examples/legacy_example.cpp` that starts with a v1.0.3 source
  file and only adds `#include "FR_legacy.h"` compiles and runs.

---

## Phase 6 — Documentation

**Goal:** README and source comments reflect v2 reality.

### Tasks

- [ ] **6.1** — Add a "v1 → v2 migration guide" section to README
  - List of renamed / removed functions
  - List of behavior changes (e.g., `FR_atan2` now returns angles)
  - Code-side examples of porting a callsite
- [ ] **6.2** — Add a "Quick reference" cheat sheet table at the top of
  the README listing every function and one-line description and
  expected error bound.
- [ ] **6.3** — Add a "Known accuracy" table with the worst-case error
  for each transcendental, copy-pasted from the test report.
- [ ] **6.4** — Add a "Platform-specific gotchas" section calling out
  the LP64 `signed long` issue (now fixed with `int32_t`) and any
  cross-compiler weirdness.
- [ ] **6.5** — Add a worked trig example: rotate a point by 45 degrees
  using the new `fr_sin` / `fr_cos` primitives.
- [ ] **6.6** — Add a worked 2D matrix example: build a rotate-then-
  translate matrix and apply it to a point.
- [ ] **6.7** — Document the print helpers with example output.
- [ ] **6.8** — Fix the `void main(void)` example to `int main(void)`.
- [ ] **6.9** — Add a table of contents to the README.
- [ ] **6.10** — Add a "Macros vs functions" guidance section explaining
  side-effect non-safety.
- [ ] **6.11** — Add a CONTRIBUTING.md (or section) describing the
  branching policy, the test-suite-as-spec convention, and how to add
  a new characterization.
- [ ] **6.12** — Add a CHANGELOG.md spanning v1.0.3 → v2.0.0.
- [ ] **6.13** — Document each shift-approximation macro inline with
  its worst-case relative error (from the table in this plan).

### Acceptance

- README `wc -l` < 600 (it's currently 234, we'll add a lot but stay
  reasonable).
- Every public function and macro has a one-line doc comment.

---

## Phase 7 — Release prep

### Tasks

- [ ] **7.1** — Bump `Version` line in README and any version macro to
  `2.0.0`.
- [ ] **7.2** — Add CI matrix entries for: GCC, Clang, Linux x64,
  Linux ARM64, optionally `arm-none-eabi-gcc -m32` cross-compile.
- [ ] **7.3** — Final pass of `make test && make test-tdd`. Diff
  `build/test_tdd_report.md` against `dev/baseline_v2_report.md` and
  bless or revert.
- [ ] **7.4** — Tag `v2.0.0`.
- [ ] **7.5** — Write a release post / GitHub release describing the
  motivation, the bugs fixed, the API changes, and the migration path.

---

## Open questions (track here, decide before each phase)

| ID | Question | Resolution |
|---|---|---|
| Q1 | Should `fr_atan2` return radians at a *fixed* radix (e.g. always 16) or a *user-supplied* radix? | TBD |
| Q2 | What is the BAM bit width? `u16` (65536 per circle) or `u32` (more precision)? **Tentative:** `u16` because cos table interp uses 7 bits, which leaves 7 bits per quadrant which is exactly the table index. Total = 16. Clean. | TENTATIVE u16 |
| Q3 | Do we make `fr_sqrt` part of the public API? It's needed for `fr_asin`/`fr_acos` derivation. **Recommendation:** yes — it's broadly useful. | TBD |
| Q4 | What's the rounding policy for `FR_printNumF`? Round-half-even (banker's), round-half-up, or truncate? | TBD |
| Q5 | Do we still expose `FR_pow10` in v2, or kill it (it's currently commented out in the .c file)? | TBD |
| Q6 | Is there an `FR_exp` we should add alongside `FR_EXP` macro? | TBD |
| Q7 | Do `fr_sin_bam(0xffff)` and `fr_sin_bam(0x0000)` give consistent results? (i.e., does the table wrap correctly at the top of the circle?) Must verify in Phase 2. | TBD |

---

## Bit-shift macro analysis (reference for Phase 6.13)

This table is the empirical accuracy of each shift-approximation macro.
Use it as the docstring source when annotating the header in Phase 6.13.

| Macro | Body sums to | True value | Abs err | Rel err | # ops |
|---|---:|---:|---:|---:|---:|
| `FR_SMUL10` | 10.000000 | 10.0 | 0 | 0 | 2 sh + 1 add |
| `FR_Q2DEG` | 90.000000 | 90.0 | 0 | 0 | 4 sh + 3 add |
| `FR_SDIV10` | 0.100098 | 0.1 | 9.8e-5 | **9.8e-4** | 5 sh + 4 add/sub |
| `FR_SrLOG2E` | 0.693146 | 0.693147 | 1.4e-6 | 2.1e-6 | 8 sh + 7 add/sub |
| `FR_SLOG2E` | 1.442688 | 1.442695 | 7.1e-6 | 4.9e-6 | 7 sh + 6 add/sub |
| `FR_SrLOG2_10` | 0.301025 | 0.301030 | 4.6e-6 | 1.5e-5 | 6 sh + 5 add/sub |
| `FR_SLOG2_10` | 3.321899 | 3.321928 | 2.9e-5 | 8.6e-6 | 8 sh + 7 add/sub |
| `FR_DEG2RAD` (body, mislabeled) | 57.295898 | 57.295780 | 1.2e-4 | 2.1e-6 | 7 sh + 6 add/sub |
| `FR_RAD2DEG` (body, mislabeled) | 0.017456 | 0.017453 | 2.8e-6 | 1.6e-4 | 3 sh + 2 add/sub |
| `FR_RAD2Q` | 0.636658 | 0.636620 | 3.8e-5 | 6.0e-5 | 5 sh + 4 add/sub |
| `FR_Q2RAD` | 1.570801 | 1.570796 | 4.5e-6 | 2.9e-6 | 5 sh + 4 add/sub |
| `FR_DEG2Q` | 0.011108 | 0.011111 | 2.7e-6 | 2.4e-4 | 4 sh + 3 add/sub |

**Verdict (from this plan's review):**
- *Excellent* (rel err < 1e-5): all log multipliers, `FR_DEG2RAD` body, `FR_Q2RAD`, `FR_SLOG2_10`.
- *Mediocre* (rel err ~1e-4): `FR_RAD2DEG` body, `FR_DEG2Q`.
- *Sloppy* (rel err ~1e-3): `FR_SDIV10`. Worth re-deriving with one more term.
- *Mislabeled*: `FR_DEG2RAD` and `FR_RAD2DEG` — bodies are swapped. **Phase 1.7 fixes this.**
- *Hygiene bug*: `FR_DEG2RAD` is missing parens around `x` in the `(x >> 4)` subexpression. **Phase 1.7 fixes this.**

---

## Notes for the implementer

- **Don't break the test suite.** Run `make test-tdd` after every commit.
  Diff the report against the previous run. Any unexpected change is a
  red flag.
- **Don't optimize before correctness.** Phase 1 is correctness-only.
  Phases 2–4 are API. Optimization comes after we have a stable baseline.
- **Don't add features beyond this plan.** If you find another bug while
  implementing, log it as a row at the bottom of this plan and finish the
  current task first.
- **Don't merge a phase that drops coverage.** Coverage should monotonically
  increase across the project.
- **Macros must dissolve when not used.** This is a user-stated constraint.
  If a BAM helper is implemented as a function, it costs callstack overhead
  even when not called. Keep them as macros.
