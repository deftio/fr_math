# FR_Math Release Notes

## Unreleased

Correctness fixes from GitHub code scanning (AI Scan alerts #9-#12), plus
the same bug class found elsewhere by the sanitizer. Results are unchanged
for every input that was already well defined: checked against 2.0.11 over
~72M inputs (all 65536 `fr_tan_bam` angles; strided full-range sweeps of
every affected function at 11-31 radices; `FR_atan2` grids; ~160k
`FR_numstr` strings), and exhaustively for `FR_RAD2BAM`. Only inputs that
hit undefined behavior change.

A second set of fixes came from testing every input instead of samples
(new `tests/test_exhaustive.c`, below). Outside the tan pole windows,
results are bit-identical to before wherever the old result was valid
(checked over ~1 billion inputs); what changes is listed here.

### Fixed

- **`fr_tan_bam`, `fr_tan`**: odd multiples of 45° read one entry past
  the 65-entry tan table (#9). Results were already correct; the
  out-of-bounds read is gone.
- **`FR_atan2`**: `INT32_MIN` in either argument overflowed when taking
  the magnitude; depending on the compiler the call hung or returned an
  angle outside [-pi, pi] (#10). It now returns the correct angle, e.g.
  `FR_atan2(INT32_MIN, INT32_MIN, r)` = -3pi/4.
- **`fr_tan_deg`**: radix 23-30 overflowed computing `360 << radix` (a
  zero divisor at radix 29-30) (#11). High radix now gives the same result
  as radix 16 for the same angle, with poles resolved at the caller's full
  precision.
- **`FR_numstr`** (behavior change for out-of-range input only): values
  that overflow `s32` wrapped, e.g. `"65536"` at radix 16 parsed as 0
  (#12). They now saturate to `FR_OVERFLOW_POS` / `FR_OVERFLOW_NEG`, as
  `FR_FixMulSat` does. Exact boundaries such as `"-32768"` at radix 16
  (= `INT32_MIN`) are still exact.
- **`INT32_MIN` input** to `fr_sin`, `fr_cos`, `fr_tan`, `fr_sin_deg`,
  `fr_cos_deg`, `fr_rad_to_bam` and `FR_pow2` overflowed on negation.
  `fr_sin_deg(INT32_MIN, 16)` returned the wrong sign (+0.139 for
  sin(-8°)); the others were right only by two's-complement accident.
- **`FR_RAD2BAM`**: the intermediate sum overflowed at -pi; terms are
  reordered as in `rad_to_bam_full` (bit-identical results for every
  input). Header comment corrected: the valid range is |x| <= pi at
  s15.16, not "~4 rad".
- **Large angles** (`fr_sin`, `fr_cos`, `fr_tan`, `fr_rad_to_bam`,
  `fr_sin_deg`, `fr_cos_deg`, `fr_deg_to_bam`): below radix 16, angles
  whose radix-16 form does not fit in `s32` overflowed and gave wrong
  answers (e.g. `fr_cos_deg(40000, 0)`); radian angles beyond ±4π also
  drifted, because they were reduced with the integer 2π at r16 (up to
  ~160 LSB at |x| ≈ 32768 rad). Degrees are now reduced mod 360 exactly at
  the caller's radix, radians with 2π to 64 bits (two multiplies, no
  divide). Every `s32` input at every radix 0..16 is now accurate.
- **Integer degrees**: `FR_DEG2BAM_I` (used by `FR_SinI`, `FR_CosI`,
  `FR_TanI` and the radix-0 paths) overflowed for |deg| >= 32768. It is
  now written as `deg * 8192 / 45`, the same result, valid to 2^18; the
  library's own integer-degree paths reduce larger values first, so
  `FR_TanI` and `fr_*_deg(deg, 0)` take any `s32`.
- **tan near its poles** (behavior change: more accurate): `fr_tan_deg`
  was up to 70% off within 0.05° of 90° / 270° (the input's BAM rounding
  amplified by 1/ε), and `fr_tan_bam` up to 0.58%. Within 10° of a pole,
  `fr_tan`, `fr_tan_deg`, `FR_TanI` and `fr_tan_bam` now return cot of the
  exact distance to the pole (1/ε − ε/3), computed with 32-bit divisions
  only (`fr_tan` previously used a 64-bit divide there). Worst error over
  every input at radix 0..16: tan (rad) 0.048%, tan (deg) 0.041%, tan
  (BAM) 0.015%. Also fixed: `fr_tan`'s pole constants at r24 were off by
  ~1 unit (π/2·2^24 = 26353589.27, the code had 26353590).
- **`FR_pow2`** at low radix: the result went through radix 16, so
  anything ≥ 2^15 overflowed even when it fits at the caller's radix
  (`FR_pow2(20, 0)` returned 0, `FR_pow2(30, 0)` returned
  `FR_OVERFLOW_POS`), and at radix 16 `FR_pow2(15.0)` wrapped to
  `INT32_MIN`. The result is now shifted once to the caller's radix and
  saturates only when it does not fit in `s32`.
- **`FR_EXP`, `FR_POW10`, `FR_EXP_FAST`, `FR_POW10_FAST`**: for very large
  |input| the base-2 exponent wrapped (`FR_EXP(INT32_MIN, 0)` returned
  `FR_OVERFLOW_POS`). It is now held to the `s32` range, which gives 0 or
  `FR_OVERFLOW_POS` exactly at radix <= 26.
- **`FR_acos`, `FR_asin`**: `INT32_MIN` overflowed on negation and returned
  π/2 instead of π (acos) / −π/2 (asin).
- **`FR_ln`, `FR_log10`** (behavior change for input <= 0): returned a
  scaled copy of `FR_LOG2MIN` (e.g. −22712.7 at radix 16) instead of the
  sentinel. They now return `FR_LOG2MIN`, as `FR_log2` does.
- **`FR_hypot`, `FR_hypot_fast8`**: results above 2^31 − 1 wrapped
  negative. They now saturate to `FR_OVERFLOW_POS` (as the docs already
  said for `FR_hypot`); `FR_hypot_fast8` works in `u32`, so `INT32_MIN`
  is taken as 2^31 instead of being clamped.

### Accuracy, measured on every input

- New `tests/test_exhaustive.c` (part of `make test`, ~20 s): every
  function at every radix 0..16 on every input in its domain (all u16 BAM
  angles; every raw value in the s16 range and in one to two full periods;
  the 65536 values at each end of `s32`; a stride across all of `s32`),
  against a double reference, with per-function error budgets. Nothing is
  skipped: zero crossings, poles, saturation and domain edges are checked
  against their documented conventions. `make test-exhaustive-s32` adds
  every one of the 2^32 inputs at radix 16 for the six angle functions.
- The published accuracy table (README, docs, site) is now measured on
  every s15.16 input in each domain, with no skipped regions (the atan2
  rows used to skip small inputs and the ±π branch cut). Some maxima rose
  because the old samples missed them, e.g. sin/cos (rad, deg) 0.15% →
  0.61% near zero crossings; others fell with the fixes, e.g. tan (deg)
  0.53% → 0.002%.
- Known limit, unchanged: `FR_EXP` / `FR_POW10` compute the base-2
  exponent at the input radix, so their error doubles for each radix bit
  removed (0.003% at radix 16, 2% at radix 4, 39% at radix 0).
- `compare_lfm/benchmark.cpp` (FR_math vs libfixmath) now also uses every
  Q16.16 input in each domain, with no skipped regions: FR_math is more
  accurate on 11 of 13 head-to-head functions (2 ties) and faster on 11.

### Build and CI

- **makefile**: `CFLAGS` / `CXXFLAGS` from the environment or command line
  were silently overridden, and library objects never used them. They now
  apply to every compile and link line.
- **CI**: because of that, the `test-32bit` job was building 64-bit and the
  `overflow-tests` job never applied its sanitizers (and could not fail:
  `make test || true`). `test-32bit` now really builds with `-m32`;
  `overflow-tests` runs the full suite under AddressSanitizer +
  UndefinedBehaviorSanitizer (clang) and fails on any report.
- **Tests**: five regression tests in `test_full_coverage.c`
  (`test_scan_*`). `test_tdd.cpp` no longer executes `FR_ABS(INT32_MIN)`
  (undefined) for its report row; the printed value is unchanged.
- **Shift overflow check**: the sanitizer job cannot use UBSan's
  shift-base check, because the library left-shifts negative values by
  design, and UBSan reports only the first problem per source line, so a
  negative shift hid a real overflow on the same line. New
  `tests/ubsan_shift_check.c` replaces the UBSan shift handler: in-range
  shifts of negative values pass, any shift that overflows aborts. `make
  test-shift` runs every suite with it; CI runs it after the sanitizer
  step. It catches the old radix-normalization overflow at once.
- **Tests**: four more regression tests (`test_angle_range_reduction`,
  `test_tan_near_pole`, `test_pow_range`, `test_edge_sentinels`) that
  fail on the previous code. `test_tdd.cpp` no longer runs `FR_DIV32` on
  inputs where its shift overflows; that row now says so.
- **Docs**: `FR_RAD2BAM` / `FR_BAM2RAD` take one argument (the reference
  showed two, and its worked example did not compile); the reference also
  described `fr_tan` as `(sin << 16) / cos`, said `FR_log2` returns
  `FR_DOMAIN_ERROR` for input <= 0 (it returns `FR_LOG2MIN`), and gave
  `FR_TanI` an `s16` argument (it is `s32`). All corrected, plus the
  changed behaviors above.

### Portability: 8- and 16-bit targets

- **Fixed (affects 2.0.11 on AVR, MSP430, 68HC11)**: where `int` is 16
  bits, constants written as `1 << n` or `1u << n` overflow for n >= 15
  (n >= 16 for `1u`). avr-gcc folds `1u << 30` to 0 (with a warning), so on
  an Arduino Uno or Nano:
  - `FR_log2`, `FR_ln` and `FR_log10` took the table index from the wrong
    bits of the mantissa and **read past the end of their 65-entry
    table** (up to entry 128), returning wrong results;
  - `FR_LOG2MIN` (`-(32767 << 16)`) evaluated to 0, so log of 0 or a
    negative number returned 0 instead of the sentinel;
  - `FR_MULK28` (used by `FR_ln`, `FR_log10`, `FR_EXP`, `FR_POW10`) lost
    its rounding term;
  - the BAM rounding term `1 << 15` in the radian and degree trig paths
    became -32768, so angles rounded the wrong way (up to 1 BAM off);
  - the degree functions' `(1 << radix) - 1` masks, and the header macros
    `FR_FRAC`, `FR_FLOOR`, `FR_CEIL`, `FR_INTERPI`, `FR_TRIG_MASK`,
    `FR2D` and `D2FR`, were wrong at radix 15 and above.

  Every such constant is now cast to `s32`, `u32` or `s64` before the
  shift. On targets with a 32-bit `int` the generated code is
  byte-identical (checked with clang -O2 for the full, core and lean
  builds of `FR_math.c` and `FR_math_2D.cpp`). There is no AVR runtime
  in the test setup, so the 16-bit fix is verified by compiling with
  avr-gcc, msp430-gcc, m68hc11-gcc and `clang --target=avr`, which now
  report no shift or overflow warnings (2.0.11 reports several).
- **Fixed (this cycle only, not in 2.0.11)**: builds with `FR_NO_STDINT`
  failed to compile, because `fr_cos_deg` and `fr_sin_deg` used
  `INT32_MIN`, which `FR_defs.h` does not define without `<stdint.h>`.
  This broke the AVR, MSP430, 68HC11, RISC-V rv32 and ESP8266 rows of the
  size report.
- **CI**: new `avr-16bit-int` job compiles every build variant with
  avr-gcc and fails on any shift or overflow warning. It fails on 2.0.11
  and passes now.

### Code size

The exact angle reduction, the cot paths near the tan poles and the
overflow fixes add code. `.text` at `-Os`, Full build (Lean / Core
change in proportion):

| Target | 2.0.11 | Now | Change |
| --- | ---: | ---: | ---: |
| Cortex-M4 | 5.5 KB | 5.9 KB | +0.4 KB |
| Cortex-M0 | 5.7 KB | 6.2 KB | +0.5 KB |
| ESP32-S3 | 6.5 KB | 6.9 KB | +0.4 KB |
| RISC-V rv32 | 6.8 KB | 7.4 KB | +0.6 KB |
| AVR ATmega328P | 15.4 KB | 16.7 KB | +1.3 KB |
| x86-64 (GCC) | 8.0 KB | 8.9 KB | +0.9 KB |

The full table in the README and `docs/building.md` is regenerated
(`make size-update`). 8- and 16-bit targets grow most because the new
64-bit multiplies, most likely, expand to library calls there.

### Documentation

- **New page: How the Approximations Work** (`docs/approximations.md`,
  `pages/guide/approximations.html`, in the site nav). A companion to the
  Fixed-Point Primer: the toolbox (range reduction, small tables with
  linear interpolation and the `h²/8 × max|f''|` error bound,
  shift-and-add constants, digit-by-digit algorithms, series near
  singularities, piecewise-linear fits), then each function family (angles
  and BAM, sin/cos, tan and its poles, inverse trig, logs, powers and exp,
  sqrt and hypot, waves), with where each error comes from and how big it
  is, how the published figures are measured, and which variant to pick.
  Linked from the primer, getting started, API reference, examples, the
  docs index and the README.
- **README, docs, site**: coverage 99% (was 98%), size tables regenerated,
  accuracy table from every input (above).
- **Build & Test page**: lists `make test-exhaustive`,
  `make test-exhaustive-s32`, `make test-shift`, the `test_exhaustive`
  binary and every CI job.
- **`llms.txt`**: degree functions take `s32` (was shown as
  `s16`), `FR_RAD2BAM` / `FR_BAM2RAD` take one argument, `FR_TanI` and
  `fr_rad_to_bam` / `fr_deg_to_bam` listed, sentinels and saturation
  rules, the `FR_EXP` low-radix limit, `hypot_fast8` error (~0.14%, was
  given as 0.10%), code size range, the new test targets. `agents.md`:
  code size range.
- **`CONTRIBUTING.md`**: the test section described four suites from an
  older layout; it now points to the full list and asks for
  `make test-shift` before PRs that touch `src/`.

### Contributors

- Alerts #9-#12 were reported by GitHub Code Security (AI Scan).
- The fixes, regression tests, verification sweeps and CI changes above,
  the every-input test suite, the shift-overflow checker, the libfixmath
  benchmark rework, the 8/16-bit portability fixes and the `avr-16bit-int`
  CI job, the new approximations page and the documentation corrections
  were made with Claude Code (Anthropic).
- Correction for 2.0.11: its CodeQL fixes and release-script consistency
  checks were also made with Claude Code; that entry did not say so.

## Version 2.0.11 (2026)

Maintenance release: GitHub code-scanning (CodeQL) cleanup. No library
or API changes; `src/` compiles to the same code as 2.0.10.

### Build and tooling

- **CI**: `ci.yml` now sets workflow-level `permissions: contents: read`,
  so the `GITHUB_TOKEN` is least-privilege for every CI job.
- **Fixed**: `tools/fr_coef-gen.cpp` passed `long` values to `%d` / `%x`
  printf specifiers; now `%ld` / `%lx`. Its coefficient comment was missing
  a `%`, so the value was never printed; restored.

## Version 2.0.10 (2026)

Optional s16-safe trig crest. Default behavior unchanged: sin/cos still
return exact ±1.0 in s15.16 (±65536).

### `FR_TRIG_S16_SAFE` (default off)

- **Added**: compile-time flag for an **s16-safe crest**. When defined
  while compiling `FR_math.c`, the quadrant-table endpoint is 32767 and
  cardinal early-outs use the same value, so the s15.16 crest is ±65534
  (2 LSB under true 1.0). Then `(s16)(x >> 1)` yields ±32767 instead of
  wrapping. Return type and radix stay s15.16 — not a q1.15 API.
- Default (flag off) keeps exact ±`FR_TRIG_ONE` (±65536). That return is
  correct; the old wrap symptom was casting into `s16`, not the library
  value itself. Leave the flag off unless a caller needs that cast.
- New example pair: `examples/trig-crest/` (`trig_crest_exact` and
  `trig_crest_s16_safe`). `waveform-synth` builds with the flag so its
  `s16` sine buffer stays in range.
- Tests cover both configs (`make test` includes `test-s16-safe`).

### Build and tooling

- **Fixed**: example Makefiles compiled `FR_math.c` with the C++ compiler,
  which clang warns about. They now build it with `$(CC)` and link with
  `$(CXX)`, and the examples build with `-Werror` (except `trig-accuracy`,
  which also compiles libfixmath). The library, tests and examples build
  with zero warnings under gcc and clang. Manual build commands in the
  example READMEs match.
- **Fixed**: `scripts/crossbuild_sizes.sh` had CRLF line endings, so
  `make size-update` failed with `bash\r`. Converted to LF.
- Size tables in `README.md`, `docs/building.md` and the building guide
  page regenerated with `make size-update`. Code size is unchanged from
  2.0.9; differences come from the current cross toolchains.

### Docs

- `FR_TRIG_S16_SAFE` documented in the build options guide.
- Examples guide lists the new `trig-crest` example.
- API reference: the trig table is the sine table `gFR_SIN_TAB_Q`, and
  `fr_cos_bam` is defined as `fr_sin_bam(bam + FR_BAM_QUADRANT)`
  (previously described the other way round).
- TDD report summary: FR_CosI/FR_SinI max error stated as ~6.6e-5
  (~4 LSB), matching the measured section-5 table.

## Version 2.0.9 (2026)

Arduino/ESP32 build fix and a saturating-add correctness fix. No API
changes — `FR_math.h` declarations are identical to 2.0.8.

### Arduino: `u8`/`u16` did not name a type on non-AVR cores

- **Fixed** ([issue #11](https://github.com/deftio/fr_math/issues/11)):
  sketches failed to compile on ESP32, and on any Arduino core other
  than AVR/SAM/SAMD, with `'u16' does not name a type`. Thanks to
  [@beaka](https://github.com/beaka) for reporting this, on ESP32 dev
  and ESP32-S3 boards with Arduino IDE 2.3.10.
- Cause: `FR_defs.h` skipped its `u8`/`u16` typedefs entirely in Arduino
  C++ builds, on the assumption that `USBAPI.h` always supplies them.
  That holds only for the AVR/SAM/SAMD cores; ESP32, RP2040, STM32 and
  others have no such typedefs, so the types were left undefined.
- Fix: in Arduino C++ builds the typedefs are now emitted using
  `USBAPI.h`'s exact underlying types (`unsigned char` /
  `unsigned short`). Where `USBAPI.h` is present the duplicate typedef
  is identical and legal C++; where it is absent the types are defined.
  Both include orders work.

### `FR_FixAddSat` returned wrong results

- **Fixed**: `FR_FixAddSat(x, y)` computed `x + y` in signed arithmetic
  and then tested the result for wraparound. Signed overflow is
  undefined behavior, so optimizers were free to delete the check — at
  `-Os`, clang did, and negative overflow returned a wrapped positive
  value instead of `FR_OVERFLOW_NEG`. The sum is now formed in unsigned
  arithmetic, which is well-defined.
- **Fixed**: `FR_FixAddSat(0, 0)` returned `FR_OVERFLOW_POS` instead of
  `0`. The positive-overflow test was `sum <= 0`; it is now `sum < 0`,
  since a zero sum from two non-negative operands is legitimate.

### Testing

- New `tests/test_arduino_compat.c` and `make test-arduino-compat`
  target (now part of `make test`, bringing the suite to eight
  binaries). It compiles the headers the way each Arduino core family
  does — ESP32-style with no `USBAPI.h` typedefs, AVR-style with them
  predefined, in both include orders, as both C and C++ — plus runtime
  type-width checks. Catches core-specific type clashes without a board
  toolchain installed.
- New branch-coverage tests in `tests/test_full_coverage.c` covering
  previously unexercised paths: radian range reduction beyond 2π and
  4π, degree reduction beyond ±360°, `radix == 0` fast paths in
  `fr_cos_deg`/`fr_sin_deg`, BAM pole handling in `fr_tan_deg`,
  radix-conversion arms in `FR_acos`/`FR_asin`/`FR_atan2` at output
  radix 14 and 20, `FR_pow2(x, 0)`, `FR_printNumF` with `radix == 0`
  and `prec == 0`, `FR_numstr` leading `+` and >9 fractional digits,
  `FR_hypot_fast8` at `INT32_MIN`, and the `fr_wave_noise` /
  `fr_adsr_step` clamps.
- Library coverage is now 98% of source lines, with every
  mathematically reachable branch exercised. The remainder is defensive
  code (clamps and guards) that no input can reach; it is kept in place
  deliberately.

### Documentation

- **Install instructions corrected**: `docs/getting-started.md`,
  `pages/guide/getting-started.html`, and `llms.txt` now show how to
  obtain the library from the package registries it is published to
  (Arduino Library Manager, PlatformIO, ESP-IDF), alongside copying the
  sources in directly. The guide previously said FR_Math had "no
  package manager integration", which conflated two separate things:
  the library indeed has no *dependencies* to install, but it is and
  was distributed through those registries — which is how the reporter
  in issue #11 obtained it.
- `docs/building.md` and `pages/guide/building.html`: test-suite table
  now lists eight binaries including `test_arduino_compat`, with an
  ESP32 `arduino-cli compile` example and a note on what the
  compatibility matrix covers.
- `agents.md` records the `FR_defs.h` Arduino typedef invariant in
  "What NOT to do", so the issue #11 fix is not undone by a future
  cleanup, and the test count is corrected to eight.
- Coverage figures refreshed across README, the guides, and `agents.md`.

## Version 2.0.8 (2026)

Tangent accuracy rewrite and trig rounding fix.

### BAM-native tangent table

- **New `fr_tan_bam(u16 bam)`** function with a dedicated 65-entry octant
  lookup table (`gFR_TAN_TAB_O` in `FR_trig_table.h`, 130 bytes ROM).
  First octant uses direct table + lerp; second octant uses the
  reciprocal identity `tan(x) = 1/tan(90-x)` with one 32-bit division.
  No 64-bit intermediates anywhere in the tan path.
- **`FR_TanI`, `FR_Tan`, `fr_tan`** are now thin wrappers over
  `fr_tan_bam`. The old sin/cos division implementation is removed.
- Peak error: 0.17% (BAM), 0.60% (deg r7), 0.17% (rad r16).

### Round-to-nearest fix for radian/degree wrappers

- `fr_cos`, `fr_sin`, `fr_tan`, `FR_Cos`, `FR_Sin`, `FR_Tan` now add
  0.5 LSB (`1 << (radix-1)`) before the `>> radix` shift when converting
  from radians/degrees to BAM. This rounds to the nearest BAM value
  instead of truncating, eliminating a systematic 1-BAM rounding error
  that caused ~1% peak error near zero crossings.
- Radian-path sin/cos/tan now match BAM-native accuracy (0.16-0.17%
  peak, was ~1.03%).

### Conversion macro trimming

- `FR_DEG2BAM`: 10 terms (~28 bits) reduced to 7 terms (~18 bits)
- `FR_RAD2BAM`: 9 terms (~27 bits) reduced to 7 terms (~21 bits)
- `FR_DEG2RAD`: 3 terms (~13 bits) extended to 5 terms (~17 bits)
- 18 bits of precision gives 4 bits of headroom over the 14-bit
  effective BAM resolution of the trig tables. Verified: reverting to
  the old full-precision macros changes sin/cos peak error by <0.04%.

### Other

- `FR_TRIG_MINVAL` fixed: was `-FR_TRIG_MASK` (-65535), now
  `-FR_TRIG_MAXVAL` (-2147483647) to properly pair with `FR_TRIG_MAXVAL`
  for tan saturation clamping.
- Accuracy table in all docs now shows separate BAM/deg/rad rows for
  sin/cos and tan, matching the TDD characterization report.
- `fr_tan_bam` added to function listings across README, docs, HTML
  pages, and llms.txt.

---

## Version 2.0.7 (2026)

README restructure, accuracy table cleanup, and expanded cross-compile support.

### New: `FR_CORE_ONLY` convenience define

A single `#define FR_CORE_ONLY` before including `FR_math.h` (or `-DFR_CORE_ONLY`
on the command line) now strips both print helpers and wave generators in one step.
It expands to `FR_NO_PRINT` + `FR_NO_WAVES` internally.

### Accuracy table cleanup

The LSB column has been removed from the accuracy table output. Percent error
is the metric that matters to callers; LSB error is an implementation detail
that varies with the chosen radix.

### Expanded cross-compile targets

- **RP2040 (Cortex-M0+)** and **STM32 (Cortex-M4)** added as named targets
  in the Docker cross-build
- **68HC11** and **MIPS32** toolchains added to the Docker image
- Size table now shows three columns: **Lean**, **Core**, and **Full**
- Consolidated `scripts/crossbuild_sizes.sh` — single script runs Docker,
  builds all targets, writes CSV + markdown, and patches doc files
  (replaces `crossbuild-docker.sh`, `size_report.sh`, `update_sizes.sh`)
- Size table sorted by architecture width (8-bit → 64-bit)

### README restructure

Sections reordered: accuracy table moved above the size table to lead with
the library's primary selling point. Badges cleaned up from Quikdown HTML to
standard markdown syntax. Build flavor descriptions made more concise.

---

## Version 2.0.6 (2026)

Accuracy improvements, lean-build options, and library cleanup.

### Accuracy & algorithms

- **FR_acos boundary fix**: deferred quantization computes `1-x` at the
  caller's radix instead of r15, giving 12x better accuracy near ±1.0
  (max LSB error 512.6 → 42.3)
- **FR_atan2 rewrite**: uses asin/acos + hypot_fast8 with octant
  switching for well-conditioned results everywhere (0.41% peak vs
  20% for libfixmath)

### Lean build options

Two new compile-time `#define` guards strip optional subsystems for
ROM-constrained targets:

| Define | Removes | Savings |
|---|---|---|
| `FR_NO_PRINT` | `FR_printNumF/D/H`, `FR_numstr` | ~1.3 KB |
| `FR_NO_WAVES` | `fr_wave_*`, `fr_adsr_*`, `FR_HZ2BAM_INC` | ~0.6 KB |

With both guards the core math library compiles to ~3.5 KB on x86-64
(clang -Os), roughly 2.6 KB on Thumb-2.

### Removed

- **FR_hypot_fast** (4-segment) deleted — FR_hypot_fast8 (8-segment)
  is strictly better in both accuracy (0.10% vs 0.34%) and is used
  internally by FR_atan2. The 4-segment variant was dead weight.

### Other

- libfixmath comparison benchmark (`compare_lfm/`) added to repo
- Documentation updated across all markdown and HTML pages

## Version 2.0.5 (2026)

Release pipeline fixes. No functional changes to the math library.

- Fixed `tools/make_release.sh` failing after squash-merge: local master
  diverges from origin (squash creates a new commit), so `git pull
  --ff-only` fails. Now detects divergence and resets to origin/master.
- Fixed on-master release path: script now pushes master to origin and
  waits for CI before tagging (previously skipped both, causing tags to
  point at commits not yet on the remote).
- Release pipeline auto-commits pipeline-generated changes (badge
  updates, version sync) instead of failing on a dirty working tree.
  Unexpected dirty files still block the release.

## Version 2.0.4 (2026)

CI fix release. No functional changes to the math library.

- Fixed `release.yml` coverage step failing due to stale gcov invocation
- Removed conflicting auto-release job from `ci.yml` (replaced by
  tag-triggered `release.yml`)
- Documentation updated: `release_management.md`, `docs/building.md`,
  and `pages/guide/building.html` now accurately describe the guided
  release pipeline

---

## Version 2.0.3 (2026)

CI and release pipeline cleanup. No functional changes to the math
library itself.

### Release pipeline

- New guided release script `tools/make_release.sh` (ported from xelp):
  validates, opens PR, waits for CI, merges, tags, waits for GitHub
  Release, then publishes to PlatformIO and ESP-IDF registries
- Removed old `scripts/make_release.sh` (validation-only, manual merge)
- All doc/page references updated to `tools/make_release.sh`
- Cleaned up stale branches

---

## Version 2.0.2 (2026)

Embedded library publishing support. No functional changes to the
math library itself — this release adds package manager integration
and improves example organization.

### Library publishing

- Added `library.properties` for Arduino Library Manager
- Added `library.json` for PlatformIO Registry
- Added `idf_component.yml` + `CMakeLists.txt` for ESP-IDF Component Registry
- Added `keywords.txt` for Arduino IDE syntax highlighting
- Added `.github/workflows/release.yml` for tag-triggered release validation

### Examples reorganized

- New focused Arduino sketches: `basic-math`, `trig-functions`, `wave-generators`
- Moved `FR_Math_Example1.cpp` to `examples/posix-example/`
- Removed symlinks from `examples/arduino_smoke/` (Arduino forbids symlinks)
- Added `examples/README.md`

### Documentation

- Added `llms.txt` — machine-readable API summary for AI coding agents
- Added `agents.md` — coding agent conventions and contribution guide
- Added `dev/misc/` publish checklists for Arduino, PlatformIO, and ESP-IDF
- Updated `scripts/sync_version.sh` to sync version across all new metadata files
- Added release step 11: verify `llms.txt` and `agents.md` are current

---

## Version 2.0.1 (2026)

Precision and accuracy release. All changes are backward-compatible
with v2.0.0 except where noted.

### Trig output precision

- `fr_cos_bam` / `fr_sin_bam` now return **s15.16** (was s0.15).
  Exact values at cardinal angles: `cos(0°) = 65536`, `cos(90°) = 0`.
  `FR_TRIG_ONE = 65536`.
- `FR_TanI` / `fr_tan` return s15.16 (was s16.15). Saturation
  is now `±INT32_MAX`.
- All wrappers updated: `FR_Cos`, `FR_Sin`, `FR_CosI`, `FR_SinI`,
  `FR_TanI`, `FR_Tan`.

### Inverse trig now returns radians

- `FR_acos`, `FR_asin`, `FR_atan` gain an `out_radix` parameter
  and return radians at that radix (was degrees as `s16`).
- `FR_atan2(y, x, out_radix)` also returns radians.
- `FR_BAM2RAD` macro corrected (was off by a factor of 1024).

### Rounding improvements

- `FR_FixMuls` / `FR_FixMulSat`: add 0.5 LSB (`+0x8000`) before
  the `>>16` shift. Both now **round to nearest** instead of
  truncating.
- `FR_sqrt` / `FR_hypot`: the internal `fr_isqrt64` now rounds to
  nearest (remainder > root → +1). Worst-case error drops from
  <1 LSB to **≤ 0.5 LSB**.
- `FR_DIV` now **rounds to nearest** (≤ 0.5 LSB error) instead of
  truncating. `FR_DIV_TRUNC` preserves the old truncating behaviour.

### Improved exp / log accuracy

- **`FR_pow2` table expanded** from 17 entries (16 segments) to
  65 entries (64 segments, 260 bytes). Interpolation error drops
  by ~16×.
- **`FR_log2` table expanded** from 33 entries to 65 entries
  (6-bit index / 24-bit interpolation). Worst-case error ≤ 4 LSB
  at Q16.16.
- **`FR_MULK28` macro** added: multiplies any fixed-point value
  by a radix-28 constant using a 64-bit intermediate with
  round-to-nearest. ~9 decimal digits of precision.
- `FR_EXP` and `FR_POW10` now use `FR_MULK28` for the base
  conversion instead of shift-only macros.
- `FR_ln` and `FR_log10` also use `FR_MULK28` internally.
- **`FR_EXP_FAST`** and **`FR_POW10_FAST`** added as shift-only
  alternatives for 8-bit targets where 64-bit multiply is
  expensive.

### New symbols

- `FR_MIN`, `FR_MAX`, `FR_CLAMP` — standard min/max/clamp.
- `FR_DIV_TRUNC(x, xr, y, yr)` — truncating division (the old
  `FR_DIV` behaviour).
- `FR_MOD(x, xr, y, yr)` — fixed-point modulus.
- Radix-28 constants: `FR_kLOG2E_28`, `FR_krLOG2E_28`,
  `FR_kLOG2_10_28`, `FR_krLOG2_10_28`.

### Breaking changes from v2.0.0

| Change | v2.0.0 | v2.0.1 |
|--------|--------|--------|
| sin/cos return type | `s16` (s0.15) | `s32` (s15.16) |
| sin/cos 1.0 value | 32767 | 65536 (exact) |
| tan return format | s16.15 (radix 15) | s15.16 (radix 16) |
| tan saturation | `±(32767 << 15)` | `±INT32_MAX` |
| FR_acos/asin signature | `(input, radix)` → s16 degrees | `(input, radix, out_radix)` → s32 radians |
| FR_atan signature | `(input, radix)` → s16 degrees | `(input, radix, out_radix)` → s32 radians |
| FR_atan2 signature | `(y, x)` → s16 degrees | `(y, x, out_radix)` → s32 radians |
| FR_BAM2RAD | off by 1024× (bug) | correct |
| FR_DIV rounding | truncates toward zero | rounds to nearest (use `FR_DIV_TRUNC` for old behaviour) |

---

## Version 2.0.0 (2026)

This is a significant bug-fix and precision-improvement release. Several
v1 functions produced wrong numerical results on every platform due to
arithmetic bugs; others produced wrong results specifically on 64-bit
hosts because of a typedef choice. Every changed function is covered by
the TDD characterization suite in `tests/test_tdd.cpp`.

See `dev/fr_math_precision.md` for the full per-symbol reference
(inputs, outputs, precision, saturation) and `dev/fr_math2_impl_plan.md`
for the implementation plan this release executed.

### 64-bit safety (portability)

- **`FR_defs.h`: migrated typedefs to `<stdint.h>`** (`s8` → `int8_t`,
  `s32` → `int32_t`, etc.). v1 defined `s32` as `signed long`, which is
  64 bits on LP64 platforms (Linux and macOS on x64 / ARM64), so every
  fixed-point computation that relied on `s32` being exactly 32 bits
  silently produced wrong answers on desktop Linux and macOS. C99
  `<stdint.h>` is now mandatory in v2 (every modern toolchain — gcc,
  clang, MSVC, IAR, Keil C51, sdcc, MSP430-gcc, AVR-gcc, RISC-V/ARM —
  ships it). If you are stuck on a pre-C99 compiler, FR_Math 1.0.x
  remains the version for you.

### Numerical fixes

- **`FR_FixMulSat` / `FR_FixMuls`**: v1 used a split-multiply formula
  with an algebraic bug that returned wrong values for certain sign
  combinations. v2 uses an `int64_t` fast path with explicit
  saturation.
- **`FR_log2`**: v1 was missing the accumulator in the mantissa-table
  step and returned wrong values for non-power-of-2 inputs. v2 rewrites
  the algorithm: leading-bit-position → normalize to s1.30 → 33-entry
  mantissa lookup with linear interpolation.
- **`FR_ln` / `FR_log10`**: inherit the `FR_log2` fix automatically
  (they are multiplies of `FR_log2` by a constant).
- **`FR_pow2`**: v1 used C truncation toward zero to extract the integer
  exponent, which gave wrong answers for negative non-integer inputs
  (e.g. `FR_pow2(-0.5)` returned ~2 instead of ~0.707). v2 uses
  mathematical floor (toward −∞) and a 17-entry fraction table with
  linear interp.
- **`FR_atan2`**: v1 was a placeholder that returned garbage. v2 is a
  correct octant-reduced arctan with a 33-entry table. Max error ≤ 1°.
  v2 also drops the vestigial `radix` parameter — new signature is
  `FR_atan2(s32 y, s32 x)`.
- **`FR_atan`**: v1 was wired up incorrectly. v2 implements as
  `FR_atan2(input, 1<<radix)`.
- **`FR_Tan`**: v1 used `s16` loop variables that overflowed for angles
  near 90°. v2 uses `s32`.
- **`FR_TanI`**: removed dead unreachable code (`if (270 == deg)`).
- **`FR_printNumD`**: v1 invoked unary minus on `INT_MIN`, which is
  undefined behavior, and always returned 0. v2 works in unsigned
  magnitude and returns the real byte count (or −1 on null `f`).
- **`FR_printNumF`**: same INT_MIN fix, plus v1's fraction extraction
  was mathematically wrong and printed bogus tail digits.
- **`FR_printNumH`**: v1 shifted a signed negative int right. v2 casts
  to unsigned first.

### Macro fixes

- **`FR_DEG2RAD` and `FR_RAD2DEG`**: v1 had the macro bodies swapped
  relative to their names, so `FR_DEG2RAD(x)` actually multiplied by
  57.3 and `FR_RAD2DEG(x)` multiplied by 0.017. v2 swaps them back.
  `FR_DEG2RAD` also had missing parens around `x` in a subexpression.
- **`FR_NUM`**: v1 signature was `FR_NUM(i, f, r)` and the body ignored
  the `f` argument entirely. v2 adds an explicit `d` digit-count
  argument — new signature is `FR_NUM(i, f, d, r)`. Any caller of the
  v1 form must be updated.

### New functionality

- **Radian-native trig** (`fr_cos`, `fr_sin`, `fr_tan`, `fr_cos_bam`,
  `fr_sin_bam`, `fr_cos_deg`, `fr_sin_deg`): takes radians (or BAM, or
  integer degrees) directly rather than forcing everything through
  integer degrees. Uses a new 129-entry s0.15 quadrant cosine table in
  `src/FR_trig_table.h` with linear interpolation and round-to-nearest.
  Max error ≤ 1 LSB of s0.15 (~3e−5). Mean error ~0.
- **BAM (Binary Angular Measure) macros**: `FR_DEG2BAM`, `FR_BAM2DEG`,
  `FR_RAD2BAM`, `FR_BAM2RAD`. BAM is the natural integer representation
  for trig: 16 bits per full circle, top 2 bits select the quadrant,
  next 7 bits index the table, bottom 7 bits drive interpolation.
- **Table size is a compile-time knob**: `-DFR_TRIG_TABLE_BITS=8` gives
  a 257-entry table for halved worst-case error (default is 7 / 129
  entries).
- **Square root and hypot** (`FR_sqrt`, `FR_hypot`): radix-aware fixed
  point square root and 2D vector magnitude. `FR_sqrt` uses a
  digit-by-digit (shift-and-subtract) integer isqrt on `int64_t`.
  Negative input returns `FR_DOMAIN_ERROR` (`INT32_MIN`). `FR_hypot`
  computes `sqrt(x^2 + y^2)` with no intermediate overflow up to the
  full s32 range. Bit-exact for perfect squares; max error ~1 LSB at
  the requested radix.
- **Fast approximate magnitude** (`FR_hypot_fast8`):
  shift-only piecewise-linear approximation of `sqrt(x^2 + y^2)` — no
  multiply, no divide, no 64-bit math, no ROM table, no iteration.
  `FR_hypot_fast8` uses 8 segments (~0.14% peak error). Based on the
  method of US Patent 6,567,777 B1 (Chatterjee, public domain). No
  `radix` parameter needed — the algorithm is scale-invariant.
- **Wave function family** for embedded audio / LFOs / control
  signals. All take a `u16` BAM phase and return s0.15 (s16, ±32767):
  - `fr_wave_sqr(phase)` — symmetric square wave.
  - `fr_wave_pwm(phase, duty)` — variable-duty pulse wave; `duty` is
    a `u16` threshold in BAM units.
  - `fr_wave_tri(phase)` — symmetric triangle, peaks clamped to
    ±32767. Max error vs ideal triangle ~3e−5 (1 LSB s0.15).
  - `fr_wave_saw(phase)` — sawtooth, `(s16)(phase - 0x8000)` with
    boundary clamp.
  - `fr_wave_tri_morph(phase, break_point)` — variable-symmetry
    triangle that morphs into a sawtooth as `break_point` approaches
    `0` or `0xffff`. Returns unipolar [0, 32767]. Uses one division
    per sample.
  - `fr_wave_noise(state)` — 32-bit Galois LFSR (poly `0xD0000001`,
    period 2^32 − 1) returning a full-range s16. Caller owns the
    `u32` state; seed with any non-zero value.
- **`FR_HZ2BAM_INC(hz, sample_rate)`**: phase increment helper.
  Computes `hz * 65536 / sample_rate` so a u16 phase accumulator
  driven by this increment produces the requested frequency. Example:
  `FR_HZ2BAM_INC(440, 48000) = 600` (≈ 439.45 Hz).
- **ADSR envelope generator** (`fr_adsr_t`, `fr_adsr_init`,
  `fr_adsr_trigger`, `fr_adsr_release`, `fr_adsr_step`): linear-segment
  attack/decay/sustain/release envelope. Internal levels are stored as
  s1.30 so very long durations (e.g. 48000-sample attack at 48 kHz)
  still get a non-zero per-sample increment; `fr_adsr_step` returns
  s0.15 for direct multiplication into a wave sample. State machine
  exposes constants `FR_ADSR_IDLE` / `_ATTACK` / `_DECAY` / `_SUSTAIN`
  / `_RELEASE`.

### New documentation

- **`dev/fr_math_precision.md`** — comprehensive per-symbol precision
  reference. Every public macro, constant, and function is documented
  with inputs, output format, worst-case error, saturation behavior,
  and side-effect notes.
- **`CONTRIBUTING.md`** — PR expectations, test discipline, portability
  rules, commit message format.
- **`tools/interp_analysis.html`** — interactive Chart.js analysis of
  trig interpolation methods. Compares nearest-neighbor, linear
  truncated, linear rounded, cosine interp, smoothstep, Hermite cubic,
  and Catmull-Rom on the same 129-entry table, over θ ∈ [−45°, +45°].
  Includes pan/zoom on a second chart showing the actual interpolated
  values vs `Math.cos` reference. Use this to verify interpolation
  claims and to pick a default for the library.
- **`scripts/clean_build.sh`** — one-shot clean of `build/` and
  `coverage/` directories. Handy when switching branches or debugging
  stale object files.

### Breaking changes

- **`FR_NUM` signature**: `FR_NUM(i, f, r)` → `FR_NUM(i, f, d, r)`. Any
  code that called the 3-argument form will fail to compile. The old
  form was broken (it ignored `f` entirely), so any caller was already
  getting wrong results.
- **`FR_atan2` signature**: `FR_atan2(y, x, radix)` → `FR_atan2(y, x)`.
  The radix parameter was vestigial (ignored by the v1 placeholder and
  unnecessary in v2).
- **`FR_RESULT` and `FR_E_*` codes removed**: v1's HRESULT-style return
  codes are gone. The matrix `inv()` methods now return `bool`
  (`true` on success, `false` if singular). `add()`, `sub()`, and
  `setrotate()` return `void`. Math functions that can hit a domain
  error return the named sentinel `FR_DOMAIN_ERROR`; saturating
  arithmetic returns `FR_OVERFLOW_POS` / `FR_OVERFLOW_NEG`.
- **`FR_SQUARE` and `FR_FIXMUL32u` removed**: these s16.16-only macros
  were narrow specializations of `FR_FixMuls`/`FR_FixMulSat`. Use the
  generic versions, which now have an `int64_t` fast path and work at
  any radix.
- **`FR_NO_INT64` and `FR_NO_STDINT` build flags removed**: every
  C99-or-newer toolchain on every architecture (including 8-bit
  targets like sdcc, AVR-gcc, MSP430-gcc) provides `<stdint.h>` and
  64-bit integer arithmetic, so the conditional fallbacks were
  carrying their weight in maintenance and ROM for no benefit.
- **Wider intermediate types**: if you had code that poked at `s32` as
  if it were `long` (e.g. printing with `%ld`), that will now warn.
  Use `%d` with `s32` / `int32_t`, or cast.

## Version 1.0.3 (2025)

### Test Coverage Improvements
- **Increased overall test coverage from 4% to 72%**
  - FR_math.c: 60% coverage
  - FR_math_2D.cpp: 98% coverage
- Added comprehensive test suite with multiple test files:
  - `test_comprehensive.c` - Comprehensive tests for core math functions
  - `test_overflow_saturation.c` - Edge case and overflow behavior tests
  - `test_full_coverage.c` - Tests for previously uncovered functions
  - `test_2d_complete.cpp` - Complete 2D transformation matrix tests
- Fixed test failures in 2D transformation tests (XFormPtI now correctly expects integer inputs)

### Bug Fixes
- **Fixed FR_atan function** - Was incorrectly calling FR_atan2 with wrong arguments, now properly calls FR_atanI
- **Fixed constant declarations** - Changed FR_PI, FR_2PI, FR_E from non-const to proper const declarations
- **Fixed XFormPtI test assumptions** - Tests were incorrectly passing fixed-point values instead of integers

### Build System Enhancements
- **Added comprehensive Makefile targets**:
  - `make lib` - Build static library
  - `make test` - Run all tests
  - `make coverage` - Generate coverage reports with lcov
  - `make examples` - Build example programs
  - `make clean` - Clean build artifacts
- **Added GitHub Actions CI/CD**:
  - Multi-platform testing (Ubuntu, macOS)
  - Multi-compiler support (gcc, clang)
  - Cross-compilation testing (ARM, RISC-V)
  - 32-bit compatibility testing
  - Automated coverage reporting to Codecov
  - Overflow and saturation testing with sanitizers

### Documentation Improvements
- **Cleaned up README.md**:
  - Fixed grammar and spelling throughout
  - Added "Building and Testing" section with clear instructions
  - Improved formatting with consistent markdown usage
  - Added supported platforms list
  - Better code examples with syntax highlighting
  - Clearer mathematical notation and explanations
- **Added CLAUDE.md** - Assistant-friendly documentation with:
  - Project structure overview
  - Key concepts and math notation explained
  - Testing and linting commands
  - Common pitfalls and tips
- **Added inline documentation** for test files explaining coverage goals

### Code Quality
- **Removed unused/unimplemented functions**:
  - Removed FR_atan declaration (was declared but never implemented)
  - Cleaned up test code to remove references to non-existent functions
- **Fixed compiler warnings**:
  - Resolved deprecated C++ warnings
  - Fixed format string warnings in tests
  - Addressed unused variable warnings
- **Improved code organization**:
  - Better separation of test types
  - Clearer test naming conventions
  - More maintainable test structure

### Platform Support
- Verified compilation and testing on:
  - x86/x64 (Linux, macOS)
  - ARM (32-bit and 64-bit)
  - RISC-V
  - 32-bit x86 targets
- Added cross-compilation support in CI

### Version Updates
- Updated all source file headers to version 1.0.3
- Added version section to README.md

## Previous Versions

### Version 1.02
- Initial public release
- Basic fixed-point math operations
- 2D transformation matrices
- Core trigonometric functions

### Version 1.01
- Internal development version
- Cleaned up naming conventions
- Initial test framework

---

*Note: FR_Math has been in development since 2000, originally used in embedded systems for Palm Pilots and later ARM cores. This is the first version with comprehensive testing and CI/CD integration.*