# Release Management

Reference for every helper script and `make` target used to build, test,
validate, and ship FR_Math. If a command is not listed here, it isn't
part of the supported release workflow.

The single source of truth for the current version is the repo-root
`VERSION` file (one line, e.g. `2.0.0`). All version-bearing files are
kept in sync from that file via `scripts/sync_version.sh`.

---

## Quick reference

| Command | Purpose |
| --- | --- |
| `make` | Build library + examples |
| `make test` | Build + run the full test suite |
| `./scripts/build.sh` | Clean rebuild + run tests (one-shot) |
| `./scripts/clean_build.sh` | Wipe `build/` and `coverage/`, recreate them |
| `./scripts/coverage_report.sh` (or `make coverage`) | gcov coverage table |
| `./scripts/size_report.sh` (or `make size-report`) | Multi-arch object-size report |
| `./scripts/sync_version.sh` | Propagate `VERSION` to every versioned file |
| `./scripts/sync_version.sh --check` | Drift check (non-destructive) |
| `./scripts/sync_version.sh --set X.Y.Z` | Bump version, then sync |
| `./scripts/make_release.sh` | Strict pre-merge release validation gate |

---

## Helper scripts (`scripts/`)

### `scripts/build.sh` — one-shot clean rebuild

Clean rebuild of library, examples, and tests. Useful when switching
branches or after invasive edits. Delegates to `clean_build.sh`,
`sync_version.sh --check` (non-blocking warning), and `make`.

| Invocation | Effect |
| --- | --- |
| `./scripts/build.sh` | Clean + build lib + build examples + build + run tests |
| `./scripts/build.sh --no-test` | Clean + build everything, skip test execution |
| `./scripts/build.sh --lib` | Clean + build library only |
| `./scripts/build.sh --help` | Print usage |

Exit status is 0 on success, non-zero on the first failing step. Prints
a host-side size report for `build/FR_math.o` and `build/FR_math_2D.o`
at the end of the build step, and a suggested follow-up list on
success.

### `scripts/clean_build.sh` — wipe and recreate build tree

Removes `build/` and `coverage/` in one shot and recreates them empty.
Also sweeps stray `*.gcda` / `*.gcno` / `*.gcov` files out of the
source tree and removes top-level `*.o`, `*.exe`, and `*.info`. Several
`make` targets write object files directly under `build/`, so removing
the directory outright (as plain `make clean` does) can leave those
targets unable to open their output — this script is the safe version.

| Invocation | Effect |
| --- | --- |
| `./scripts/clean_build.sh` | Wipe and recreate `build/` + `coverage/` |
| `./scripts/clean_build.sh --all` | Same, plus remove `*~` editor backups |

### `scripts/coverage_report.sh` — gcov coverage table

Builds `FR_math.c` and `FR_math_2D.cpp` with `-fprofile-arcs
-ftest-coverage`, runs every test binary, then parses the gcov output
into an ASCII table broken down per source file plus an overall
percentage. Has no external dependency beyond `gcc` / `gcov` (no
`lcov`). Also prints the top untested functions in `FR_math.c`.

Wiped build state: this script calls `rm -rf build coverage` at the
start, so do not run it inside a session that depends on pre-existing
`build/` contents.

Invoked automatically by `make coverage` and by
`scripts/make_release.sh`.

### `scripts/size_report.sh` — multi-architecture size report

Compiles `src/FR_math.c` against every cross-toolchain it can find and
prints a formatted table of object sizes. Architectures attempted:

- `x86-32` (`gcc -m32`)
- `x86-64` (`gcc -m64`)
- `ARM32` (`arm-linux-gnueabihf-gcc`)
- `ARM64` (`aarch64-linux-gnu-gcc`)
- `Cortex-M0` (`arm-none-eabi-gcc -mcpu=cortex-m0 -mthumb`)
- `Cortex-M4` (`arm-none-eabi-gcc -mcpu=cortex-m4 -mthumb`)
- `68k` (`m68k-elf-gcc`)
- `RISC-V 32` (`riscv64-unknown-elf-gcc -march=rv32imc -mabi=ilp32`)
- `RISC-V 64` (`riscv64-unknown-elf-gcc -march=rv64imac -mabi=lp64`)
- Native (`gcc`)

Missing toolchains are silently reported as `not found` — the script
does not fail. On a successful native build it also prints an
optimisation comparison (`-O0` / `-Os` / `-O2` / `-O3`) for the host
platform. Tail of the output lists the package names used to install
the cross toolchains on Debian/Ubuntu and macOS.

Invoked automatically by `make size-report`.

### `scripts/sync_version.sh` — version propagation

Single source of truth is the repo-root `VERSION` file. This script
rewrites every file that embeds a user-visible version string so they
all match.

Files it updates:

- `README.md` — shields.io version badge (`img.shields.io/badge/version-X.Y.Z-…`)
- `README.md` — the `Current version: X.Y.Z` line in the Version section
- `pages/assets/site.js` — `var FR_VERSION = 'vX.Y.Z';` (shown in the docs site header)
- `src/FR_math_2D.h` — `@version X.Y.Z` doxygen tag
- `src/FR_math_2D.cpp` — `@version X.Y.Z` doxygen tag
- `scripts/make_release.sh` — the `git tag -a vX.Y.Z …` hint in the squash-merge instructions

Files it deliberately leaves alone (history / descriptive prose):

- `release_notes.md` — canonical change log; maintained by hand
- `pages/releases.html` — release history page
- `pages/**/*.html` — dated "As of vX.Y.Z" observations
- `docs/**/*.md` — markdown mirror of `pages/`

| Invocation | Effect |
| --- | --- |
| `./scripts/sync_version.sh` | Propagate current `VERSION` to every file above |
| `./scripts/sync_version.sh --check` | Drift check only; exit 1 if any file disagrees |
| `./scripts/sync_version.sh --set=X.Y.Z` | Rewrite `VERSION` first, then propagate |
| `./scripts/sync_version.sh X.Y.Z` | Same as `--set`, positional form |
| `./scripts/sync_version.sh --help` | Print usage |

Idempotent: running it when everything is already in sync is a no-op
and prints `All files already at X.Y.Z. Nothing to do.`

### `scripts/make_release.sh` — strict release validation gate

Full pre-merge validation. Does **not** push, tag, or merge anything —
it runs a strict build + full test suite + coverage + size report and
then prints squash-merge instructions to follow by hand. Exit status is
0 only if every step passes.

Steps, in order:

1. **Clean build tree** — calls `clean_build.sh`.
2. **Version sync check** — runs `sync_version.sh --check`; aborts on drift.
3. **Strict compile** — rebuilds `FR_math.c` and `FR_math_2D.cpp` with
   `-Wall -Wextra -Werror -Wshadow -Os`. Any warning is a release blocker.
4. **Build library + examples** — `make lib examples`.
5. **Run full test suite** — `make test`. Also scans the log for
   `Failed: N` lines with `N ≥ 1` and aborts if any are found.
6. **Coverage report** — runs `coverage_report.sh`; parses the overall
   percentage. Warns (but does not fail) if coverage is below 90%.
7. **Update README badges** — rewrites the `coverage-<N>%25-<color>` and
   `tests-<N>%20passing-brightgreen` badge URLs in `README.md` from the
   measured numbers. Picks a shields.io colour based on the coverage
   percentage. If the badges change, prints a reminder to commit.
8. **Size report** — `size` (or `ls`) on the host objects.
9. **Cross-compile sanity** — tries `arm-none-eabi-gcc`,
   `riscv64-unknown-elf-gcc`, and `arm-linux-gnueabi-gcc`. If a
   toolchain is installed, it must compile cleanly; otherwise it is
   silently skipped. `--skip-cross` disables this step entirely.
10. **Working tree status** — `git status --porcelain`; warns if the
    tree is dirty (it can legitimately be dirty from the badge update
    in step 7).

Final output is a summary (branch, commit, tests passed, coverage) plus
a squash-merge checklist showing the exact `git` / `gh` commands to
commit any remaining changes, push, open the PR, squash-merge, and tag.

| Invocation | Effect |
| --- | --- |
| `./scripts/make_release.sh` | Run the full ten-step validation |
| `./scripts/make_release.sh --skip-cross` | Skip step 9 (faster on dev machines without cross toolchains) |
| `./scripts/make_release.sh --help` | Print usage |

---

## Make targets (`makefile`)

Run from the repo root.

### Build

| Target | Effect |
| --- | --- |
| `make` / `make all` | Create `build/` + `coverage/` dirs, build library, build examples |
| `make dirs` | Create `build/` and `coverage/` directories |
| `make lib` | Compile `build/FR_math.o` and `build/FR_math_2D.o` |
| `make examples` | Build `build/fr_example` from `examples/FR_Math_Example1.cpp` |

### Test

`make test` runs every suite below in one shot. Each target can also be
invoked individually.

| Target | Binary | Source |
| --- | --- | --- |
| `make test` | *(all suites)* | — |
| `make test-basic` | `build/fr_test` | `tests/fr_math_test.c` |
| `make test-comprehensive` | `build/test_comprehensive` | `tests/test_comprehensive.c` |
| `make test-2d` | `build/test_2d` | `tests/test_2d_math.c` |
| `make test-overflow` | `build/test_overflow` | `tests/test_overflow_saturation.c` |
| `make test-full` | `build/test_full` | `tests/test_full_coverage.c` |
| `make test-2d-complete` | `build/test_2d_complete` | `tests/test_2d_complete.cpp` |
| `make test-tdd` | `build/test_tdd` | `tests/test_tdd.cpp` — characterisation suite; report written to `build/test_tdd_report.md` |

### Coverage

| Target | Effect |
| --- | --- |
| `make coverage` | Delegates to `scripts/coverage_report.sh` (gcov table, no external deps) |
| `make coverage-basic` | Clean rebuild with coverage flags, then a minimal gcov "Lines executed" summary |
| `make coverage-html` | Full lcov-based HTML report at `coverage/html/index.html`. Requires `lcov` (`brew install lcov` / `apt-get install lcov`). |

### Size

| Target | Effect |
| --- | --- |
| `make size-report` | Delegates to `scripts/size_report.sh` (multi-arch table) |
| `make size-simple` | `size` (or `ls -lh`) on `build/*.o` for the current platform only |

### Clean

| Target | Effect |
| --- | --- |
| `make clean` | Remove `build/` and `coverage/`, plus top-level `*.o`, `*.gcda`, `*.gcno`, `*.exe`, `*.info`. Note: if you then invoke a target that writes directly into `build/`, use `scripts/clean_build.sh` instead so the directory is recreated. |
| `make cleanall` | `make clean` plus remove `*~` editor backups in the root, `src/`, and `tests/` |

---

## Release workflow

A typical version bump + release runs these commands in order:

```bash
# 1. Bump the source-of-truth version and propagate it everywhere.
./scripts/sync_version.sh --set=2.1.0

# 2. Update release_notes.md by hand — sync_version.sh does not touch it.
$EDITOR release_notes.md

# 3. Run the full validation gate. This also rewrites the README badges
#    from the measured coverage/tests numbers.
./scripts/make_release.sh

# 4. Review, commit, push, PR, squash-merge, tag — the script prints
#    the exact git / gh commands for each step at the end.
```

For day-to-day development the full gate is overkill; the common loop is:

```bash
./scripts/build.sh           # clean rebuild + tests
./scripts/coverage_report.sh # coverage after a change
./scripts/size_report.sh     # size after a change
```

---

## See also

- `CONTRIBUTING.md` — PR expectations, portability rules, commit format.
- `release_notes.md` — canonical per-release change log.
- `VERSION` — single source of truth for the current version string.
- `docs/building.md` — user-facing build instructions.
