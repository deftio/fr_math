# Security Policy

FR_Math is a small, dependency-free C library that is often compiled
into firmware where it cannot easily be updated later. We take reports
of memory-safety and undefined-behavior bugs seriously, even when they
look like "just a math bug".

## Supported versions

Security fixes go into the latest release on the 2.x line. Older
releases do not get backports; upgrade to the latest 2.0.x release
(the API is stable within 2.x).

| Version | Supported |
|---------|-----------|
| 2.0.x (latest) | Yes |
| < 2.0 | No |

## Reporting a vulnerability

**Please do not open a public issue, pull request or discussion for a
security problem.**

Report it privately through GitHub:

1. Go to the repository's **Security** tab.
2. Click **Report a vulnerability**
   (direct link: <https://github.com/deftio/fr_math/security/advisories/new>).
3. Describe the problem and how to reproduce it.

If you cannot use GitHub, email the maintainer at the address in
`LICENSE.txt` with "FR_Math security" in the subject line.

A useful report includes:

- the FR_Math version or commit,
- the function, its arguments and the radix,
- the compiler, flags and target (for example `arm-none-eabi-gcc -Os`,
  Cortex-M0, or x86-64 with `-fsanitize=undefined`),
- what happens (crash, sanitizer report, out-of-bounds read, hang) and
  what you expected,
- a minimal reproducer, ideally a few lines of C.

## What to expect

This project is maintained by one person, so the timelines below are
targets, not guarantees:

- **Acknowledgement** within 7 days.
- **Assessment** (confirmed or not, and how severe) within 14 days.
- **Fix** in a release within 90 days of the report for confirmed
  issues, usually much sooner.

We coordinate disclosure with you. Once a fix is released, we publish a
GitHub Security Advisory and, where it applies, request a CVE. Reporters
are credited in the advisory and in `release_notes.md` unless they ask
not to be.

## Scope

In scope: the library itself, `src/` (`FR_math.c`, `FR_math.h`,
`FR_defs.h`, `FR_math_2D.h`, `FR_math_2D.cpp`). For
example:

- out-of-bounds reads or writes, such as a table index past its end,
- undefined behavior in C for any input the documentation says is
  accepted: signed overflow, shifts out of range, division by zero,
- infinite loops or unbounded run time for any input,
- the print and parse helpers (`FR_printNum*`, `FR_numstr`) reading or
  writing out of bounds.

Out of scope:

- accuracy within the published error budgets (see `README.md` and
  `docs/`); a result that is outside its budget is a bug, so please
  open a normal issue for it,
- code under `examples/`, `tests/`, `tools/`, `dev/`, `docker/`,
  `compare_lfm/` and `pages/`, which is not shipped as part of the
  library; a bug there is still welcome as a normal issue,
- vulnerabilities in the compiler, the C runtime or third-party code.

If you are not sure whether something is a security problem, report it
privately anyway. We would rather triage a false alarm than read about
a real issue in public first.

## How we keep the library safe

- The full test suite runs in CI on gcc and clang, and again under
  UndefinedBehaviorSanitizer with integer-overflow checks.
- GitHub code scanning (CodeQL) runs on every push and weekly.
- The library has no dependencies; GitHub Actions used by CI are pinned
  to commit SHAs and kept up to date by Dependabot.
