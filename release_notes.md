# FR_Math Release Notes

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
- **Fast approximate magnitude** (`FR_hypot_fast`, `FR_hypot_fast8`):
  shift-only piecewise-linear approximation of `sqrt(x^2 + y^2)` — no
  multiply, no divide, no 64-bit math, no ROM table, no iteration.
  `FR_hypot_fast` uses 4 segments (~0.4% peak error);
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