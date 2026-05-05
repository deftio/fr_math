# Release Management

Reference for every helper script and `make` target used to build, test,
validate, and ship FR_Math. If a command is not listed here, it isn't
part of the supported release workflow.

The single source of truth for the current version is
`FR_MATH_VERSION_HEX` in `src/FR_math.h` (hex `0xMMmmpp` → `M.m.p`).
All version-bearing files are kept in sync via
`scripts/sync_version.sh`.

---

## Quick reference

| Command | Purpose |
| --- | --- |
| `make` | Build library + examples |
| `make test` | Build + run the full test suite |
| `./scripts/build.sh` | Clean rebuild + run tests (one-shot) |
| `./scripts/clean_build.sh` | Wipe `build/` and `coverage/`, recreate them |
| `./scripts/coverage_report.sh` (or `make coverage`) | gcov coverage table |
| `./scripts/crossbuild_sizes.sh` (or `make size-report`) | Multi-arch object-size report (Docker) |
| `./scripts/sync_version.sh` | Propagate `FR_MATH_VERSION_HEX` to every versioned file |
| `./scripts/sync_version.sh --check` | Drift check (non-destructive) |
| `./tools/make_release.sh` | Guided release pipeline (validate → PR → merge → tag → publish) |
| `./tools/make_release.sh --validate` | Local validation only (build, tests, coverage) |

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
`tools/make_release.sh`.

### `scripts/crossbuild_sizes.sh` — multi-architecture size report

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

Single source of truth is `FR_MATH_VERSION_HEX` in `src/FR_math.h`.
This script rewrites every file that embeds a user-visible version
string so they all match.

Files it updates:

- `src/FR_math.h` — `FR_MATH_VERSION` string (derived from `_HEX`)
- `VERSION` — plain-text `X.Y.Z` file
- `README.md` — shields.io version badge (`img.shields.io/badge/version-X.Y.Z-…`)
- `README.md` — the `Current version: X.Y.Z` line
- `pages/assets/site.js` — `var FR_VERSION = 'vX.Y.Z';` (docs site header)
- `src/FR_math_2D.h` — `@version X.Y.Z` doxygen tag
- `src/FR_math_2D.cpp` — `@version X.Y.Z` doxygen tag
- `library.properties` — Arduino Library Manager `version=` field
- `library.json` — PlatformIO registry `"version":` field
- `idf_component.yml` — ESP-IDF Component Registry `version:` field
- `llms.txt` — `- Version: X.Y.Z` line

Files it deliberately leaves alone (history / descriptive prose):

- `release_notes.md` — canonical change log; maintained by hand
- `pages/releases.html` — release history page
- `pages/**/*.html` — dated "As of vX.Y.Z" observations
- `docs/**/*.md` — markdown mirror of `pages/`

Manual review required at release time (no auto-sync):

- `llms.txt` — verify API docs match any new/changed functions
- `agents.md` — verify build/contribution instructions are current

| Invocation | Effect |
| --- | --- |
| `./scripts/sync_version.sh` | Propagate `FR_MATH_VERSION_HEX` to every file above |
| `./scripts/sync_version.sh --check` | Drift check only; exit 1 if any file disagrees |
| `./scripts/sync_version.sh --help` | Print usage |

Idempotent: running it when everything is already in sync is a no-op
and prints `All files already at X.Y.Z. Nothing to do.`

### `tools/make_release.sh` — guided release pipeline

End-to-end release automation. Walks through local validation, PR
creation, CI wait, merge, tagging, and package-registry publishing.
Each step pauses for confirmation before anything visible to others.
Exit status is 0 only if every step passes.

Steps (full mode):

1. **Extract version** — reads `FR_MATH_VERSION_HEX` from `src/FR_math.h`.
2. **Sync manifests** — runs `sync_version.sh --check`; auto-syncs if
   drift is detected and stages the changes.
3. **Local validation** — clean build, strict compile
   (`-Wall -Wextra -Werror -Wshadow`), `make test`, coverage report,
   README badge update, and host size report. Aborts on any warning
   or test failure.
4. **Cross-compile sanity** — tries `arm-none-eabi-gcc`,
   `riscv64-unknown-elf-gcc`, and `arm-linux-gnueabi-gcc`. Missing
   toolchains are silently skipped. `--skip-cross` disables this step.
5. **Commit pipeline-generated changes** — if only known files are dirty
   (README.md badges, version-synced manifests), prompts to commit them.
   If unexpected files are dirty, stops with an error.
6. **Check git state** — identifies the current branch, checks whether
   the tag already exists, and verifies the working tree is clean.
7. **Push branch** — syncs with `origin/master`, pushes the feature
   branch (skipped if already on master).
8. **Open PR** — creates a PR via `gh pr create` (or reuses an
   existing one).
9. **Wait for CI** — polls `gh pr checks` every 30 s until all checks
   pass (or fails immediately on a red check).
10. **Merge PR** — enables auto-merge (squash) via `gh pr merge --auto`.
11. **Switch to master** — waits for the merge to land, then checks out
    master and pulls.
12. **Verify on master** — clean rebuild + `make test` on the merged
    master.
13. **Tag and push** — creates an annotated tag `vX.Y.Z` and pushes it,
    triggering the `release.yml` workflow.
14. **Wait for GitHub Release** — polls until `release.yml` creates the
    GitHub Release (or reports a failure with recovery instructions).
15. **Publish to PlatformIO** — `pio pkg publish . --no-interactive`
    (skipped if `pio` is not installed or version already published).
16. **Publish to ESP-IDF** — `compote component upload --name fr_math
    --namespace deftio` (skipped if `compote` is not installed).
17. **Done** — prints a summary with links and an Arduino reminder.

All output is tee'd to `build/release-<timestamp>.log`.

With `--validate`, only steps 1–4 run (local validation only — nothing
is pushed, tagged, or published).

| Invocation | Effect |
| --- | --- |
| `./tools/make_release.sh` | Full guided release (recommended) |
| `./tools/make_release.sh --validate` | Local validation only (steps 1–4) |
| `./tools/make_release.sh --skip-cross` | Skip cross-compile step (faster on machines without cross toolchains) |
| `./tools/make_release.sh --help` | Print usage |

---

## Make targets (`makefile`)

Run from the repo root.

### Build

| Target | Effect |
| --- | --- |
| `make` / `make all` | Create `build/` + `coverage/` dirs, build library, build examples |
| `make dirs` | Create `build/` and `coverage/` directories |
| `make lib` | Compile `build/FR_math.o` and `build/FR_math_2D.o` |
| `make examples` | Build `build/fr_example` from `examples/posix-example/FR_Math_Example1.cpp` |

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
| `make size-report` | Delegates to `scripts/crossbuild_sizes.sh` (multi-arch table, Docker) |
| `make size-simple` | `size` (or `ls -lh`) on `build/*.o` for the current platform only |

### Clean

| Target | Effect |
| --- | --- |
| `make clean` | Remove `build/` and `coverage/`, plus top-level `*.o`, `*.gcda`, `*.gcno`, `*.exe`, `*.info`. Note: if you then invoke a target that writes directly into `build/`, use `scripts/clean_build.sh` instead so the directory is recreated. |
| `make cleanall` | `make clean` plus remove `*~` editor backups in the root, `src/`, and `tests/` |

---

## Release workflow

A typical version bump + release:

```bash
# 1. Create a feature branch.
git checkout -b release-2.1.0

# 2. Bump the version hex in src/FR_math.h and propagate it.
#    Edit FR_MATH_VERSION_HEX, then:
./scripts/sync_version.sh

# 3. Update release_notes.md, docs/releases.md, and
#    pages/releases.html by hand — sync_version.sh does not touch them.
$EDITOR release_notes.md docs/releases.md pages/releases.html

# 4. Verify llms.txt and agents.md reflect any API changes.

# 5. Commit all changes and run the guided release pipeline.
#    This handles validation, PR, CI wait, merge, tagging,
#    GitHub Release, PlatformIO, and ESP-IDF publishing.
./tools/make_release.sh
```

For local validation only (no PR, no tagging, no publishing):

```bash
./tools/make_release.sh --validate
```

For day-to-day development the full pipeline is overkill; the common
loop is:

```bash
./scripts/build.sh           # clean rebuild + tests
./scripts/coverage_report.sh # coverage after a change
./scripts/crossbuild_sizes.sh     # size after a change
```

---

## See also

- `CONTRIBUTING.md` — PR expectations, portability rules, commit format.
- `release_notes.md` — canonical per-release change log.
- `src/FR_math.h` — `FR_MATH_VERSION_HEX`, the single source of truth
  for the version.
- `docs/building.md` — user-facing build instructions.
