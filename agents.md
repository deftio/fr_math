# agents.md — Coding Agent Guide for FR_Math

This file helps AI coding agents (Claude Code, Copilot, Cursor, Cody,
etc.) work effectively with this codebase. For a machine-readable API
summary, see [llms.txt](llms.txt).

## Project overview

FR_Math is a pure-C fixed-point math library for embedded systems.
Integer-only, zero dependencies, caller-selectable radix (binary point).

## Repository layout

```
src/                  Core library (this is what ships)
  FR_math.h           Public API — all macros, function declarations, constants
  FR_math.c           All function implementations (trig tables inlined)
  FR_defs.h           Type aliases (s8, s16, s32, u8, u16, u32)
  FR_math_2D.h/.cpp   Optional C++ 2D transform class

tests/                Test suite (7 programs, run via `make test`)
examples/             Arduino .ino sketches + POSIX example
docs/                 Markdown documentation
pages/                HTML documentation (mirrors docs/)
scripts/              Build, release, version sync, size report helpers
tools/                Coefficient generators (Python, C++)
dev/                  Development notes and planning (not shipped)
```

## Build and test

```bash
make lib              # compile library objects
make test             # run all 7 test suites (27+ tests)
make examples         # build example programs
make size-report      # cross-compile size report (Docker)
make size-update      # size report + patch doc files
make clean            # remove build artifacts
```

All tests must pass before committing. Zero compiler warnings required.

## Conventions — follow these when writing code

### Naming
- **Functions**: `FR_` prefix for public C functions, `fr_` for lowercase-style functions
- **Macros**: ALL UPPERCASE with `FR_` prefix (e.g., `FR_ADD`, `FR_CLAMP`)
- **Types**: Use `s32`, `u16`, etc. from `FR_defs.h` — never raw `int` or `unsigned`

### Fixed-point pattern
Most functions take a `radix` parameter specifying the binary point position.
Always pass radix explicitly; never hardcode a radix inside library functions.

```c
s32 result = FR_sqrt(I2FR(2, R), R);  // R is the caller's chosen radix
```

### Adding a new function
1. Declare in `src/FR_math.h` inside the `extern "C"` block
2. Implement in `src/FR_math.c`
3. Add tests in `tests/test_full_coverage.c` (pass/fail) and/or `tests/test_tdd.cpp` (characterization)
4. Update `keywords.txt` (KEYWORD2 for functions, LITERAL1 for macros)
5. Update `llms.txt` with the new function signature
6. Update `docs/api-reference.md` and `pages/guide/api-reference.html`

### Documentation mirroring
`docs/*.md` and `pages/guide/*.html` contain the same content in different
formats. When updating documentation, update both. The HTML files are
hand-authored (no build step).

## Version management

Single source of truth: `FR_MATH_VERSION_HEX` in `src/FR_math.h`.

```bash
# After editing FR_MATH_VERSION_HEX:
./scripts/sync_version.sh          # propagate to all versioned files
./scripts/sync_version.sh --check  # verify everything is in sync
```

Versioned files (all synced automatically):
- `src/FR_math.h`, `VERSION`, `README.md`, `pages/assets/site.js`
- `src/FR_math_2D.h`, `src/FR_math_2D.cpp`
- `library.properties`, `library.json`, `idf_component.yml`, `llms.txt`

## Release checklist (abbreviated)

1. Bump `FR_MATH_VERSION_HEX` in `src/FR_math.h`
2. Run `./scripts/sync_version.sh`
3. Run `./scripts/crossbuild_sizes.sh --update` (rebuild size tables)
4. Run `./scripts/accuracy_report.sh --update` (rebuild accuracy tables)
5. Run `./tools/make_release.sh` (full validation gate)
6. Verify `llms.txt` and `agents.md` are current with any API changes
7. Commit, tag, push

## Lean build options

Define before including `FR_math.h` to exclude optional subsystems:

| Define | Removes | Savings |
|---|---|---|
| `FR_CORE_ONLY` | Print + waves (shorthand for both below) | ~1.9 KB |
| `FR_NO_PRINT` | `FR_printNumF/D/H`, `FR_numstr` | ~1.3 KB |
| `FR_NO_WAVES` | `fr_wave_*`, `fr_adsr_*`, `FR_HZ2BAM_INC` | ~0.6 KB |

## Platform targets

The library compiles on: AVR (Arduino), ARM Cortex-M0/M4, ESP32,
RISC-V, x86/x64, MSP430, m68k, PowerPC, MIPS32, 68HC11. Code is 3–9 KB at `-Os` on
32-bit targets.

## Library publishing

FR_Math is published to multiple package registries:

| Registry | Metadata file | Auto-publish trigger |
|---|---|---|
| Arduino Library Manager | `library.properties` | GitHub release tag |
| PlatformIO | `library.json` | GitHub release tag |
| ESP-IDF Component Registry | `idf_component.yml` + `CMakeLists.txt` | GitHub release tag |

## What NOT to do

- Don't add floating-point dependencies (the whole point is integer-only)
- Don't use `float` or `double` in library source (tests/examples are OK)
- Don't change the `extern "C"` wrapping in `FR_math.h`
- Don't hardcode a radix inside library functions — always parameterize
- Don't add `#include` dependencies beyond `<stdint.h>`
