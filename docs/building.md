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

There is no Autotools, no Ninja, no package-manager integration.
The primary build system is a single Makefile. A `CMakeLists.txt`
exists for ESP-IDF integration only — it is not a general-purpose
CMake build.

## Makefile targets

| Target | What it builds |
| --- | --- |
| `make` (default) | Library + examples. |
| `make lib` | Library only (`build/libfrmath.a`). |
| `make examples` | Example programs into `build/`. |
| `make test` | Build every test binary and run the full suite. |
| `make test-exhaustive` | Every function at every radix 0..16 on every input in its domain, against a double reference (part of `make test`, ~20 s). Report: `build/test_exhaustive_report.md`. |
| `make test-exhaustive-s32` | Adds every one of the 2^32 inputs at radix 16 for the six angle functions (~10 min). |
| `make test-shift` | Every suite rebuilt with clang and the shift-overflow checker in `tests/ubsan_shift_check.c`. |
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

Tests live under `tests/` and are split into nine
binaries to keep compile times low:

| Binary | What it checks |
| --- | --- |
| `fr_test` | Radix conversions, `FR_ADD`, `FR_FixMuls`, rounding (legacy harness). |
| `test_comprehensive` | Trig (degree, radian, BAM), log/exp, sqrt, hypot. |
| `test_2d` | 2D transforms, determinants, inverses. |
| `test_overflow` | Overflow sentinels, saturation, edge radixes. |
| `test_full` | Full-coverage dark-corner cases and round-trips. |
| `test_2d_complete` | Extended 2D: matrix composition, inverse, point transforms. |
| `test_tdd` | Characterization tests pinned to bit-exact reference values. |
| `test_arduino_compat` | Arduino build compatibility: compiles the headers as ESP32-style and AVR/USBAPI-style cores would (both include orders), plus type-size checks (issue #11). |
| `test_exhaustive` | Every input, every radix 0..16, every function, with per-function error budgets; nothing skipped (zero crossings, poles, domain edges, both ends of `s32`). |

The suite covers **99%** of the library source lines and every
mathematically reachable branch. The remainder is defensive code
(clamps and guards) that no input can trigger, kept in place as
insurance against future edits. Every public symbol is exercised
at least once.

### Running a single binary

```bash
make test-comprehensive
./build/test_comprehensive

# or all of them at once
make test
```

### Running the TDD pins after a change

`test_tdd.cpp` is a characterization suite. It records
exact bit patterns for a sample of inputs and fails loudly if those
patterns drift. Any change that modifies the numerical behavior of
the library will break this suite — that's the point.

If you *intended* to change the numerical behavior (e.g.
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
| RP2040 (Cortex-M0+) | `arm-none-eabi-gcc` | Docker. |
| STM32 (Cortex-M4) | `arm-none-eabi-gcc` | Docker. |
| RISC-V rv64 / rv32 | `riscv64-linux-gnu-gcc`, `riscv64-unknown-elf-gcc` | Docker. |
| AVR (ATmega328P, ATtiny85) | `avr-gcc` | CI (compile, 16-bit-int warnings) + Docker. |
| Arduino (AVR, SAMD, etc.) | `arduino-cli` | Manual. |
| MSP430 | `msp430-gcc` | Docker. |
| Motorola 68k | `m68k-linux-gnu-gcc` | Docker. |
| Motorola 68HC11 | `m68hc11-gcc` | Docker. |
| PowerPC | `powerpc-linux-gnu-gcc` | Docker. |
| MIPS32 | `mipsel-linux-gnu-gcc` | Docker. |
| Xtensa LX106 (ESP8266) | `xtensa-lx106-elf-gcc` | Docker. |
| Xtensa LX7 (ESP32-S3) | `xtensa-esp-elf-gcc` | Docker (Espressif toolchain). |
| 8051 | `sdcc` | Manual. |

### Code size (.text section, compiled with `-Os`)

Sizes are for `FR_math.c` compiled with `-Os`.
Lean = `-DFR_LEAN -DFR_NO_PRINT` (radian trig, inv trig, log/exp, sqrt).
Core = `-DFR_CORE_ONLY` (+ degree trig, BAM tan, log10, hypot).
Full = all features (+ print, waves, ADSR).
With `-ffunction-sections` and linker `--gc-sections`, only the
functions your application references are linked, so real flash
usage will be smaller.

<!-- SIZE_TABLE_START -->
| Target | Lean | Core | Full |
|--------|-----:|-----:|-----:|
| AVR ATmega328P | 9.3 KB | 13.8 KB | 16.8 KB |
| AVR ATtiny85 | 9.6 KB | 14.4 KB | 17.3 KB |
| 68HC11 | 13.6 KB | 19.3 KB | 24.0 KB |
| MSP430 | 8.1 KB | 11.7 KB | 14.2 KB |
| Cortex-M4 (STM32) | 3.4 KB | 4.8 KB | 6.0 KB |
| Cortex-M0 (RP2040) | 3.6 KB | 5.0 KB | 6.3 KB |
| ARM Thumb | 3.6 KB | 5.1 KB | 6.5 KB |
| Xtensa LX7 (ESP32-S3) | 4.0 KB | 5.6 KB | 6.9 KB |
| RISC-V rv32 | 4.2 KB | 6.0 KB | 7.4 KB |
| Xtensa LX106 (ESP8266) | 4.4 KB | 6.3 KB | 8.0 KB |
| ARM32 | 4.5 KB | 6.3 KB | 8.3 KB |
| m68k | 4.4 KB | 6.7 KB | 8.4 KB |
| x86-32 | 5.5 KB | 7.7 KB | 9.9 KB |
| MIPS32 | 5.8 KB | 8.2 KB | 10.6 KB |
| PowerPC | 6.0 KB | 8.7 KB | 11.3 KB |
| TCC x86 | 8.1 KB | 11.9 KB | 15.7 KB |
| RISC-V rv64 | 4.3 KB | 6.0 KB | 7.5 KB |
| x86-64 (GCC) | 5.1 KB | 6.9 KB | 8.9 KB |
| AArch64 (ARM64) | 5.1 KB | 7.3 KB | 9.5 KB |
| x86-64 (Clang) | 4.8 KB | 7.0 KB | 9.5 KB |
<!-- SIZE_TABLE_END -->

### Lean build options

Compile-time `#define` guards let you strip optional subsystems
for ROM-constrained targets. Define them before including `FR_math.h`
(or pass `-D` on the compiler command line):

| Define | What it removes | Typical savings |
|---|---|---|
| `FR_LEAN` | Degree trig, BAM tan, angle converters, `FR_log10`, `FR_hypot`, waves + ADSR | ~3.7 KB |
| `FR_CORE_ONLY` | Print + waves (shorthand for both below) | ~1.9 KB |
| `FR_NO_PRINT` | `FR_printNumF`, `FR_printNumD`, `FR_printNumH`, `FR_numstr` | ~1.3 KB |
| `FR_NO_WAVES` | `fr_wave_*` (6 shapes), `fr_adsr_*` (ADSR envelope), `FR_HZ2BAM_INC` | ~0.6 KB |

`FR_LEAN` keeps only radian trig (sin, cos, tan), inverse trig, sqrt,
log2, ln, exp, pow2, and arithmetic — comparable to libfixmath's API at
4.8 KB text (x86-64, clang -Os). `FR_LEAN` implies `FR_NO_WAVES`.

`FR_CORE_ONLY` is a convenience shorthand that defines both
`FR_NO_PRINT` and `FR_NO_WAVES` in one step.

`FR_TRIG_S16_SAFE` (default off) removes nothing; it changes the sin/cos
crest from exact ±65536 to ±65534 so a blind `(s16)(x >> 1)` cast yields
±32767 instead of wrapping. Results stay s15.16. The sine table lives in
`FR_math.c`, so the flag must be set when compiling that file (`-D` on
the command line or in `build_flags`); a `#define` in your own source
has no effect. See `examples/trig-crest/`.

```c
/* Example: headless sensor node — math only, no print, no audio */
#define FR_CORE_ONLY
#include "FR_math.h"
```

With `-ffunction-sections` and linker `--gc-sections`, the linker will
also strip any unused functions automatically, so these guards are most
useful when you include the library as a single `.c` file or static
archive without section-level dead-code elimination.

To regenerate this table, run the Docker cross-build
(requires the [xelp](https://github.com/deftio/xelp) Docker image):

```bash
scripts/crossbuild_sizes.sh
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

# Non-AVR cores work the same way, e.g. ESP32:
arduino-cli compile --fqbn esp32:esp32:esp32 examples/arduino_smoke
```

`make test-arduino-compat` (part of `make test`) simulates the
Arduino build environments on the host — ESP32-style cores with no
USBAPI.h typedefs and AVR-style cores that predefine `u8`/`u16`,
in both include orders — so core-specific type clashes like
issue #11 are caught without any board toolchain installed.

See the [code size table](#code-size-text-section-compiled-with--os) above
for exact numbers. With linker dead-code elimination, only the
functions you call are linked.

## CI

GitHub Actions runs on every push and every PR. The workflow file
is `.github/workflows/ci.yml`, and it caches nothing: each run starts
from a clean checkout. Its jobs:

| Job | What it does |
| --- | --- |
| `build-and-test` | `make test` with gcc and clang on Linux, clang on macOS, strict warnings; coverage report. |
| `cross-compile` | Builds the library for ARM and RISC-V Linux. |
| `avr-16bit-int` | Compiles every build variant with `avr-gcc` (16-bit `int`) and fails on any shift or overflow warning, or if any lookup table lands in RAM (`.data` / `.rodata`) instead of flash. |
| `test-32bit` | `make test` built with `-m32`. |
| `overflow-tests` | `make test` under AddressSanitizer + UndefinedBehaviorSanitizer (clang), then `make test-shift`. Any report fails the job. |

GitHub code scanning (CodeQL) also runs on every push and weekly. To run
the same queries locally before pushing, use `make codeql` (needs the
CodeQL CLI; see `scripts/codeql_local.sh`).

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
