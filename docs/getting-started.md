# Getting Started

This page takes you from `git clone` to a running program
in under five minutes. If you want to understand *why*
fixed-point math works the way it does first, read the
[Fixed-Point Primer](fixed-point-primer.md) and come
back.

---

## Install

FR_Math is a handful of C and C++ files. There is no package
manager integration and no install step. Either:

- Copy `src/FR_math.c`, `src/FR_math.h`,
  `src/FR_defs.h` (and optionally
  `src/FR_math_2D.cpp`, `src/FR_math_2D.h`,
  and `src/FR_trig_table.h`) into your project, **or**
- Add FR_Math as a git submodule and point your build system at
  `src/`.

There are no external dependencies beyond `<stdint.h>`,
which every C99-or-newer toolchain ships.

```bash
git clone https://github.com/deftio/fr_math.git
cd fr_math
./scripts/build.sh
```

`build.sh` wipes `build/`, rebuilds the
library, examples, and tests, and runs the full test suite. On success
you will see 40 tests pass across six test binaries.

---

## Your first program

Save this as `hello_fr.c`:

```c
#include <stdio.h>
#include "FR_defs.h"
#include "FR_math.h"

int main(void)
{
    /* We want to represent 3.14159 in a signed 32-bit integer
       with 16 fractional bits (s15.16 format). */
    const int radix = 16;

    /* I2FR(integer, radix) converts a whole-number int into its
       fixed-point bit pattern at the requested radix. */
    s32 pi_int  = I2FR(3, radix);                   /* = 3 << 16 */
    s32 pi_frac = (s32)(0.14159 * (1 << radix));   /* build-time float */

    s32 pi_fixed = pi_int + pi_frac;
    printf("pi as s15.16 = 0x%08x = %d.%06d\n",
           pi_fixed,
           FR_FR2I(pi_fixed, radix),                /* integer part */
           FR_FR2I((pi_fixed & ((1 << radix) - 1)) * 1000000, radix));

    /* Multiply pi by 2 at the same radix using FR_FixMuls. */
    s32 two_pi = FR_FixMuls(pi_fixed, I2FR(2, radix), radix);
    printf("2*pi ~= %d.%06d\n",
           FR_FR2I(two_pi, radix),
           FR_FR2I((two_pi & ((1 << radix) - 1)) * 1000000, radix));

    return 0;
}
```

Compile and run:

```bash
cc -Isrc hello_fr.c src/FR_math.c -lm -o hello_fr
./hello_fr
```

Expected output:

```
pi as s15.16 = 0x0003243f = 3.141586
2*pi ~= 6.283172
```

The last decimal digit wobbles because we only kept 16 bits of
fractional precision — that's roughly 5 significant decimal
digits, which matches what we see. This is the core trade of
fixed-point: **you pick the precision at build time**,
and you live with it.

---

## Using the 2D transform module

```c
#include <stdio.h>
#include "FR_math.h"
#include "FR_math_2D.h"

int main(void)
{
    FR_Matrix2D_CP M;
    M.setidentity();        /* identity at the default radix */
    M.setrotate(45);        /* rotate 45 degrees */
    M.settranslate(I2FR(100, 16), I2FR(50, 16));

    FR_2DPoint p = { I2FR(10, 16), I2FR(0, 16) };
    M.XformPt(&p);          /* p becomes ~(107, 57) */

    printf("x=%d y=%d\n",
           FR_FR2I(p.x, 16),
           FR_FR2I(p.y, 16));
    return 0;
}
```

See [Examples](examples.md) for audio, wave,
envelope, and log/exp walkthroughs.

---

## Running the test suite

```bash
make test           # build + run every test suite
make coverage       # coverage report (requires gcov)
```

As of v2.0.0, FR_Math ships with 40 passing tests and 97% line
coverage across the library sources.

---

## Next steps

- **[Fixed-Point Primer](fixed-point-primer.md)**
  — how the radix notation and the library's naming
  conventions work.
- **[API Reference](api-reference.md)**
  — per-symbol inputs, outputs, precision, and saturation
  behaviour.
- **[Examples](examples.md)** —
  runnable snippets for common tasks.
- **[Building & Testing](building.md)**
  — build system, cross-compilation, coverage, release
  validation.
