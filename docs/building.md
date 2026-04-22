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

### `tools/make_release.sh`

Guided release pipeline. Handles everything from local validation
through PR, merge, tagging, GitHub Release, and package-registry
publishing (PlatformIO and ESP-IDF). Each outward-facing step
pauses for confirmation.

```bash
./tools/make_release.sh              # full guided release
./tools/make_release.sh --validate   # local validation only
./tools/make_release.sh --skip-cross # skip cross-compile step
```

In full mode the pipeline runs 17 steps: extract version, sync
manifests, strict compile + tests + coverage + badges, cross-compile
sanity, commit pipeline-generated changes, check git state, push
branch, open PR, wait for CI, merge PR, switch to master, verify
on master, tag, wait for GitHub Release, publish to PlatformIO,
publish to ESP-IDF, done.

With `--validate`, only the local validation steps run (steps 1–4)
— nothing is pushed, tagged, or published.

See `release_management.md` for the full step-by-step reference.

## The test suite

Tests live under `tests/` and are split into six
binaries to keep compile times low:

| Binary | What it checks |
| --- | --- |
| `test_basic` | Radix conversions, `FR_ADD`, `FR_FixMuls`, rounding. |
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
is an integer pipeline and `<stdint.h>` (or define `FR_NO_STDINT`
for bare-metal toolchains that lack it — `FR_defs.h` provides
fallback typedefs). You do *not* need a floating-point unit, and
you do *not* need `libm`.

| Target | Toolchain | Tested? |
| --- | --- | --- |
| x86 / x86_64 Linux | `gcc`, `clang`, `tcc` | CI + Docker. |
| macOS arm64 / x86_64 | Apple `clang` | CI. |
| Windows x86_64 | MSVC, `clang-cl`, MinGW | Manual. |
| AArch64 (ARM64) | `aarch64-linux-gnu-gcc` | Docker. |
| ARM32 / Thumb | `arm-none-eabi-gcc`, IAR, Keil | Docker. |
| RISC-V rv64 / rv32 | `riscv64-linux-gnu-gcc`, `riscv64-unknown-elf-gcc` | Docker. |
| AVR (ATmega328P, ATtiny85) | `avr-gcc` | Docker. |
| Arduino (AVR, SAMD, etc.) | `arduino-cli` | Manual. |
| MSP430 | `msp430-gcc` | Docker. |
| Motorola 68k | `m68k-linux-gnu-gcc` | Docker. |
| Motorola 68HC11 | `m68hc11-gcc` | Docker. |
| PowerPC | `powerpc-linux-gnu-gcc` | Docker. |
| Xtensa LX106 (ESP8266) | `xtensa-lx106-elf-gcc` | Docker. |
| 8051 | `sdcc` | Manual. |

### Code size (.text section, compiled with `-Os`)

All sizes are for the complete `FR_math.c` — every function
included, nothing stripped.  With `-ffunction-sections` and
linker `--gc-sections`, only the functions your application
references are linked, so real flash usage will be smaller.

<!-- SIZE_TABLE_START -->
| Target | .text (bytes) |
|---|---:|
| GCC ARM32 Thumb | 4,278 |
| GCC RISC-V (rv64) | 4,574 |
| GCC RISC-V (rv32) | 4,820 |
| GCC Xtensa LX106 (ESP8266) | 5,317 |
| GCC m68k | 5,410 |
| GCC ARM32 | 5,504 |
| GCC x86-64 | 5,857 |
| GCC AArch64 (ARM64) | 6,112 |
| Clang x86-64 | 6,555 |
| GCC x86-32 | 6,947 |
| GCC PowerPC | 7,540 |
| GCC MSP430 | 9,146 |
| TCC x86 | 9,887 |
| GCC AVR5 (ATmega328P) | 10,806 |
| GCC AVR ATtiny85 | 11,382 |
| GCC 68HC11 | 16,392 |
<!-- SIZE_TABLE_END -->

### Lean build options

Two compile-time `#define` guards let you strip optional subsystems
for ROM-constrained targets. Define them before including `FR_math.h`
(or pass `-D` on the compiler command line):

| Define | What it removes | Typical savings |
|---|---|---|
| `FR_NO_PRINT` | `FR_printNumF`, `FR_printNumD`, `FR_printNumH`, `FR_numstr` | ~1.3 KB |
| `FR_NO_WAVES` | `fr_wave_*` (6 shapes), `fr_adsr_*` (ADSR envelope), `FR_HZ2BAM_INC` | ~0.6 KB |

With both guards enabled the core math library (trig, inverse trig, log/exp,
sqrt, hypot) compiles to ~3.5 KB on x86-64 / clang -Os.

```c
/* Example: headless sensor node — math only, no print, no audio */
#define FR_NO_PRINT
#define FR_NO_WAVES
#include "FR_math.h"
```

With `-ffunction-sections` and linker `--gc-sections`, the linker will
also strip any unused functions automatically, so these guards are most
useful when you include the library as a single `.c` file or static
archive without section-level dead-code elimination.

To regenerate this table, run the Docker cross-build
(requires the [xelp](https://github.com/deftio/xelp) Docker image):

```bash
scripts/crossbuild-docker.sh
```

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

See the [code size table](#code-size-text-section-compiled-with--os) above
for exact numbers. With linker dead-code elimination, only the
functions you call are linked.

## CI

GitHub Actions runs on every push and every PR. The workflow file
is `.github/workflows/ci.yml`. It builds with strict
warnings, runs `make test`, and caches nothing —
each run starts from a clean checkout.

The CI status badge on the README links directly to the latest
run.

## Release checklist

1. Create a feature branch and bump `FR_MATH_VERSION_HEX` in
   `src/FR_math.h`.
2. Run `./scripts/sync_version.sh` to propagate the version to all
   manifests (Arduino, PlatformIO, ESP-IDF, docs).
3. Update `release_notes.md`, `docs/releases.md`, and
   `pages/releases.html` by hand.
4. Verify `llms.txt` and `agents.md` reflect any API changes.
5. Commit everything and run `./tools/make_release.sh`. The script
   handles validation, PR, CI wait, merge, tagging, GitHub Release
   creation (via `release.yml`), and publishing to PlatformIO and
   ESP-IDF registries.

Arduino Library Manager indexes from GitHub tags automatically once
the library is registered.

See [Releases](releases.md) for the list of tagged
versions and their highlights.
