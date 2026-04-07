# Contributing to FR_Math

Thanks for your interest in contributing. FR_Math is a small, focused,
embedded-targeted fixed-point math library. We want to keep it small,
fast, and numerically predictable — so the bar for new features is
"does every embedded C programmer want this?" and the bar for bug
fixes is "are you making something that used to work wrong now work
right?"

## Quick start

```bash
git clone https://github.com/deftio/fr_math.git
cd fr_math
make test          # build and run every test suite
make clean         # remove build artifacts
bash scripts/clean_build.sh   # full wipe of build/ and coverage/
```

All tests should pass on a clean checkout. If they don't, stop and
file an issue — we don't accept PRs on top of a broken baseline.

## What we welcome

- **Bug fixes** backed by a failing test case that now passes.
- **New test cases** for edge-case behavior that isn't yet pinned down.
- **Platform ports** (new compilers, new architectures) — especially
  if you can demonstrate the test suite passing on the new target.
- **Precision improvements** to existing functions, with before/after
  measurements from `tests/test_tdd.cpp`.
- **Documentation fixes** in `README.md`, `dev/fr_math_precision.md`,
  or inline in headers.
- **Size/speed optimizations** for specific embedded targets, gated
  behind `#ifdef` so the generic path stays readable.

## What we're cautious about

- **New public functions**: we try to keep the API surface small.
  Open an issue to discuss before writing the code. Ask "is this
  already composable from existing primitives?"
- **Dependencies**: the library intentionally has zero dependencies.
  Anything that requires `<math.h>`, `<stdio.h>`, dynamic allocation,
  or a C++ runtime is a hard no for the core library.
- **Portability breaks**: the library is meant to compile on
  tiny C89 toolchains (8051, MSP430, AVR, old ARM, …) as well as
  modern x64/ARM64 hosts. Don't use GCC extensions, don't assume
  two's complement unless you check, don't use VLAs.
- **Style rewrites**: we're not going to accept a PR that moves
  braces around or renames variables for aesthetic reasons.

## Ground rules for pull requests

### 1. Every behavior change needs a test

The TDD characterization suite (`tests/test_tdd.cpp`) is the
contract. If you change what a function returns for any input, you
must:

1. Find (or add) the test case that pins the old behavior.
2. Update the expected value to the new behavior.
3. Explain in your commit message *why* the old value was wrong.

If you can't find the test case for the behavior you're changing,
that's a sign you should add one *first* (in a separate commit) and
then change the code in the next commit.

### 2. No silent precision changes

If your change affects the worst-case error of any function listed
in `dev/fr_math_precision.md`, update that document in the same PR.
Precision is part of the public API.

### 3. 64-bit safety

`s32` must be exactly 32 bits on every target. Do NOT use
`long` or `long int` in new code — use `s32` / `int32_t` / `s16` /
etc. so the library works identically on LP64 (Linux/macOS x64 and
ARM64) and ILP32 (most embedded targets).

Rationale: v1 of this library used `signed long` for `s32`, which
was 64 bits on LP64 and caused silent wrong answers in
`FR_FixMulSat`, `FR_log2`, and others. We fixed this in v2 and we
don't want to regress.

### 4. Portable shift semantics

Right-shift of a negative integer is implementation-defined in
C99/C11 (it only became guaranteed arithmetic in C23). For the
library to be truly portable, we avoid shifting signed negative
values where possible. When you need round-to-nearest on a possibly
negative quantity, prefer the pattern:

```c
/* Rewrite to shift only non-negative values: */
s32 d = a - b;                    /* arrange so d >= 0 */
result = a - ((d * frac + HALF) >> BITS);
```

over

```c
/* Depends on arithmetic right-shift of negatives: */
result = a + (((b - a) * frac + HALF) >> BITS);
```

See `src/FR_math.c:fr_cos_bam` for a worked example.

### 5. Side-effect-safe macros

All public macros must wrap their arguments in parentheses AND must
not evaluate arguments with side effects incorrectly. Document the
evaluation count in the header comment:

```c
/* FR_FOO(x): ...
 * Side-effect note: x is referenced 3 times; do not pass an
 * expression with side effects.
 */
#define FR_FOO(x) ((x) + ((x) >> 1) - ((x) >> 3))
```

### 6. Commit message format

```
component: short imperative summary (<= 72 chars)

Longer explanation if needed. Wrap at 72 chars. Explain what
changed and WHY, not just what. Reference the test case that
caught the bug or that you added for the fix.

Fixes #123  (if applicable)
```

Examples of good summary lines:
- `fr_cos_bam: switch to round-to-nearest interpolation`
- `FR_log2: fix missing accumulator in mantissa lookup`
- `FR_defs.h: use stdint.h types for 64-bit safety`

Avoid: `update stuff`, `fix`, `wip`, `clean up code`.

## Running tests

```bash
make test
```

This builds and runs four test suites:

1. **Basic tests** (`tests/fr_math_test.c`) — original smoke tests
2. **Comprehensive** (`tests/test_comprehensive.c`) — adds coverage
   for all public functions
3. **2D complete** (`tests/test_2d_complete.cpp`) — matrix transforms
4. **TDD characterization** (`tests/test_tdd.cpp`) — the behavior
   contract; emits `build/test_tdd_report.md` with detailed numeric
   comparisons against `libm` for every function

All four must pass. The TDD report is your friend: if you're
wondering whether a change hurt precision somewhere, diff the
report before and after your change.

### Coverage

```bash
make coverage    # runs tests and emits coverage data
```

We track coverage via `lcov`/`coveralls`. Don't let line coverage
drop when adding new code — add tests.

## Adding a new function

Rough workflow:

1. **Open an issue** describing what you want to add and why the
   existing functions aren't sufficient.
2. **Write the test case first** in `tests/test_tdd.cpp`. Decide
   what inputs, what expected outputs, and what tolerance.
3. **Declare the function** in the appropriate header, with a
   complete doc comment covering input ranges, output format,
   precision, saturation behavior, and side effects.
4. **Implement** in the matching `.c`/`.cpp` file.
5. **Document** in `dev/fr_math_precision.md` (add an entry in the
   appropriate section).
6. **Verify** all existing tests still pass.
7. **Submit PR** with a link to the issue.

## Generating tables and constants

Precomputed tables (trig, atan, log2, pow2) are generated by
`tools/coef-gen.py`. If you modify a table, regenerate with:

```bash
python3 tools/coef-gen.py
```

and commit the regenerated `.h` file along with the script change.
Don't hand-edit table values — the script is the source of truth.

## License

By contributing you agree that your work will be released under the
same BSD 2-Clause license as the rest of the library. Don't include
code that you didn't write unless it is compatibly licensed and
attributed.

## Code of conduct

Be kind. Assume good faith. Disagree about numerics all you want —
disagree with precision and sources — but keep it on the code.

## Questions?

Open an issue. We'd rather answer a question up front than merge a
PR that has to be reverted.
