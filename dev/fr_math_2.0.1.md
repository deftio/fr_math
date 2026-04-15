# FR_Math 2.0.1 Development Checklist

Version: 2.0.1 (`FR_MATH_VERSION_HEX = 0x020001`)
Branch: `update_wave_fns`

## Completed

- [x] Doc audit — fixed 14 errors across FR_math.h, FR_math.c, docs/*.md, pages/guide/*.html
  - tan return type s15.16 → s16.15
  - FR_FR2I → FR2I, FR_MUL removed, FR_EXP/FR_POW10 casing
  - 2D examples: FR_Matrix2D_CPT, ID(), XFormPtI
  - Summary tables: added FR_hypot_fast, FR_hypot_fast8, FR_numstr
- [x] Version bump 2.0.0 → 2.0.1 (hex 0x020000 → 0x020001)
- [x] sync_version.sh rewrite — FR_MATH_VERSION_HEX is single source of truth
- [x] CI auto-release job — reads hex version, creates GitHub release + tag
- [x] Branch protection on master (CI required, no PR requirement)
- [x] Dependabot config (github-actions only, quarterly)
- [x] make_release.sh updated for new CI flow
- [x] Docker cross-compile size report (9 targets, README table updated)
- [x] FR_MIN, FR_MAX, FR_CLAMP macros added + tested
- [x] FR_DIV, FR_MOD macros added (32-bit, overflow note in comment)
- [x] **sin/cos output changed from s0.15 to s15.16** (radix 16)
  - fr_cos_bam / fr_sin_bam now return s32 at radix 16
  - Exact values at cardinal angles: cos(0°) = 65536, cos(90°) = 0, etc.
  - FR_TRIG_ONE = 65536 = exact 1.0 representation
  - tan returns s15.16 (was s16.15), saturates to ±INT32_MAX
  - All wrappers updated: FR_Cos, FR_Sin, FR_CosI, FR_SinI, FR_TanI, FR_Tan
  - FR_math_2D.cpp updated for new output precision
  - All 42 tests updated and passing
- [x] **Inverse trig changed to output radians** at caller-specified radix
  - FR_acos(input, radix, out_radix) → s32 radians at out_radix
  - FR_asin(input, radix, out_radix) → s32 radians at out_radix
  - FR_atan(input, radix, out_radix) → s32 radians at out_radix
  - FR_atan2(y, x, out_radix) → s32 radians at out_radix
  - Internal: table index → BAM → radians via FR_BAM2RAD
  - atan helper rewritten to output BAM instead of degrees
- [x] **FR_BAM2RAD macro fixed** — was off by factor of 1024 (shift was `16-r`, should be `26-r`)
- [x] Parabolic sine — **skipped** (table+lerp is accurate enough, not worth adding)

## Remaining TODO

*All items complete.*

### Documentation (updated for all the changes above)

- [x] Update docs/api-reference.md — s15.16 trig output, inverse trig radians, new macros
- [x] Update pages/guide/api-reference.html (mirror)
- [x] Update docs/README.md operations summary table
- [x] Update pages/index.html operations summary
- [x] Update docs/fixed-point-primer.md — FR_DIV/FR_MOD now exist
- [x] Update docs/getting-started.md examples (trig return type change)
- [x] Update pages/guide equivalents (getting-started.html, fixed-point-primer.html)
- [x] Update docs/examples.md and pages/guide/examples.html (trig s0.15 → s15.16)

## Breaking Changes from v2.0.0

| Change | v2.0.0 | v2.0.1 |
|--------|--------|--------|
| sin/cos return type | `s16` (s0.15) | `s32` (s15.16) |
| sin/cos 1.0 value | 32767 | 65536 (exact) |
| tan return format | s16.15 (radix 15) | s15.16 (radix 16) |
| tan saturation | `±(32767 << 15)` | `±INT32_MAX` |
| FR_acos/asin signature | `(input, radix)` → s16 degrees | `(input, radix, out_radix)` → s32 radians |
| FR_atan signature | `(input, radix)` → s16 degrees | `(input, radix, out_radix)` → s32 radians |
| FR_atan2 signature | `(y, x)` → s16 degrees | `(y, x, out_radix)` → s32 radians |
| FR_BAM2RAD | off by 1024x (bug) | correct |
| FR_TRIG_MAXVAL | 32767 (sin/cos max) | INT32_MAX (tan saturation) |

## Research Notes

### libfixmath comparison (updated)

| Feature | libfixmath | FR_math v2.0.1 | Notes |
|---------|-----------|----------------|-------|
| Fixed format | Q16.16 only | Configurable radix | FR_math more flexible |
| Angle format | Radians (Q16.16) | BAM (u16) | BAM is better for shift-only |
| sin/cos output | Q16.16 | s15.16 | **Parity** — same precision |
| Exact poles | Approximate | **Exact** | FR_math special-cases cardinal angles |
| Inverse trig | Radians | **Radians** (was degrees) | Now symmetric |
| Utility macros | Full set | Full set | min/max/clamp/div/mod added |
| Multiply-free | No | Yes (shift macros) | FR_math unique strength |

### Key design decisions
- Table stays s16[129] (258 bytes) — no change to ROM footprint
- Output is `<<1` from table values, poles are special-cased for exact 1.0
- BAM is the internal angle format — no irrational constants needed
- No CORDIC — iterative, doesn't fit shift-only philosophy
- No parabolic sine — table+lerp is already better accuracy at same cost
