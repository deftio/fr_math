# Building & Testing

FR_Math has no dependencies beyond a C99 compiler and
`<stdint.h>`. The provided build system wraps
`make` with a few convenience scripts.

## Requirements

- A C99 compiler (`gcc`, `clang`, MSVC, IAR,
  Keil, sdcc, AVR-gcc, MSP430-gcc, RISC-V gcc).
- A C++98 compiler if you want the 2D transform module
  (`FR_math_2D.cpp`).
- `make` (GNU make recommended).
- Optional: `lcov` / `gcov` for coverage
  reports.

There is no Autotools, no CMake, no Ninja, no package-manager
integration. The library is small enough that the Makefile fits on
one screen.

## Makefile targets

| Target | What it builds |
| --- | --- |
| `make` (default) | Library + examples. |
| `make lib` | Library only (`build/libfrmath.a`). |
| `make examples` | Example programs into `build/`. |
| `make test` | Build every test binary and run the full suite. |
| `make coverage` | Build with `-ftest-coverage -fprofile-arcs`, run tests, emit lcov report. |
| `make clean` | Remove `build/`. |
| `make cleanall` | Remove `build/` plus editor backups. |

All builds land in `build/`. Nothing is written inside
`src/`.

## Convenience scripts

### `scripts/build.sh`

One-shot clean rebuild.

```bash
./scripts/build.sh              # wipe build/, build lib + examples + tests, run tests
./scripts/build.sh --no-test    # skip running the test suite
./scripts/build.sh --lib        # library only
```

Returns non-zero on any failure. Suitable for use from a watch
script or a pre-commit hook.

### `scripts/coverage_report.sh`

Full gcov/lcov pipeline.

```bash
./scripts/coverage_report.sh
```

Cleans `build/`, instruments every source, runs every
test binary (including `test_tdd` for the characterization
suite), and emits an HTML report under
`build/coverage/index.html`. Also prints per-file line
coverage to the console so you can copy the numbers into badge
updates or release notes.

### `scripts/make_release.sh`

The release-candidate validator. Run it before opening a merge
request to `master`.

```bash
./scripts/make_release.sh
```

Nine steps, each gated: if any step fails, the script stops and
tells you what to fix.

1. Clean `build/`
2. Strict compile: `-Wall -Wextra -Werror -Wshadow -Wformat=2`
3. Build library + examples
4. Full test suite
5. Coverage report
6. Update README badges (coverage % and test count)
7. Size report (`size build/libfrmath.a`)
8. Cross-compile sanity check (if a cross compiler is on
   `PATH`)
9. Working-tree status (warns if there are uncommitted files other
   than the README badge update itself)

At the end, the script prints a squash-merge checklist you can
paste into a PR description. It does *not* push, tag, or merge
on its own.

## The test suite

Tests live under `tests/` and are split into six
binaries to keep compile times low:

| Binary | What it checks |
| --- | --- |
| `test_basic` | Radix conversions, `FR_ADD`, `FR_MUL`, rounding. |
| `test_trig` | Integer-degree trig (`FR_Sin` et al.). |
| `test_trig_radians` | Radian / BAM trig and the v2 `fr_sin` API. |
| `test_log_exp` | Log base 2 / ln / log10 and their inverses. |
| `test_2d` | 2D transforms, determinants, inverses. |
| `test_full_coverage` | Dark-corner cases: overflow sentinels, edge radixes, round-trips. |
| `test_tdd` | Characterisation tests pinned to bit-exact reference values. |

As of v2.0.0 the suite contains **42 tests** across
those binaries and covers **99%** of the library source.
Every public symbol is exercised at least once.

### Running a single binary

```bash
make build/test_basic
./build/test_basic

# or all of them at once
make test
```

### Running the TDD pins after a change

`test_tdd.cpp` is a characterisation suite. It records
exact bit patterns for a sample of inputs and fails loudly if those
patterns drift. Any change that modifies the numerical behaviour of
the library will break this suite — that's the point.

If you *intended* to change the numerical behaviour (e.g.
you improved a polynomial approximation), update the pinned values in
`tests/test_tdd.cpp` and note the change in
`release_notes.md` along with any updates to the
[API reference](api-reference.md) precision entries.

## Cross-compilation

The library has no CPU-specific code. It compiles and runs
identically on all of the targets listed below. The only requirement
is an integer pipeline and the standard `<stdint.h>`
header. You do *not* need a floating-point unit, and you do
*not* need `libm`.

| Target | Toolchain | Tested? |
| --- | --- | --- |
| x86 / x86_64 Linux | `gcc`, `clang` | CI. |
| macOS arm64 / x86_64 | Apple `clang` | CI. |
| Windows x86_64 | MSVC, `clang-cl`, MinGW | Manual. |
| ARM Cortex-M0/M3/M4/M7 | `arm-none-eabi-gcc`, IAR, Keil | Manual. |
| RISC-V rv32imc | `riscv32-unknown-elf-gcc` | Manual. |
| AVR (ATmega328P, etc.) | `avr-gcc` | Manual. |
| Arduino (AVR, SAMD, etc.) | `arduino-cli` | Manual. |
| MSP430 | `msp430-elf-gcc` | Manual. |
| 8051 | `sdcc` | Manual. |

### Example: RISC-V

```bash
riscv32-unknown-elf-gcc -Os -ffunction-sections -fdata-sections \
  -march=rv32imc -mabi=ilp32 \
  -Isrc -c src/FR_math.c -o FR_math.o

riscv32-unknown-elf-size FR_math.o
```

### Example: AVR

```bash
avr-gcc -Os -mmcu=atmega328p \
  -Isrc -c src/FR_math.c -o FR_math.avr.o

avr-size FR_math.avr.o
```

### Example: Arduino

```bash
arduino-cli compile --fqbn arduino:avr:uno examples/arduino_smoke

# Or try the focused examples:
arduino-cli compile --fqbn arduino:avr:uno examples/basic-math
arduino-cli compile --fqbn arduino:avr:uno examples/trig-functions
arduino-cli compile --fqbn arduino:avr:uno examples/wave-generators
```

Expect the whole integer-only library to land around a few
kilobytes of flash. The wave, trig, and log modules can be compiled
in independently if you want to strip further.

## CI

GitHub Actions runs on every push and every PR. The workflow file
is `.github/workflows/ci.yml`. It builds with strict
warnings, runs `make test`, and caches nothing —
each run starts from a clean checkout.

The CI status badge on the README links directly to the latest
run.

## Release checklist

Before tagging a release:

1. Update `release_notes.md` with what changed.
2. Run `./scripts/make_release.sh` and fix any failures.
3. Commit the README badge update (if the script produced one).
4. Squash-merge the branch to `master` using the
   message template the script prints.
5. Tag `master` with the version and push the tag.
6. Draft a GitHub release pointing at the tag, copy-pasting the
   top section of `release_notes.md`.

See [Releases](releases.md) for the list of tagged
versions and their highlights.
