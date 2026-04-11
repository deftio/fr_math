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
you will see 42 tests pass across six test binaries.

---

## A first program

The example below converts a decimal constant to fixed-point, multiplies
it, and prints the result — all in a handful of lines. It uses a
desktop `printf` for output; the next section shows the embedded
equivalent.

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

## The same program for embedded targets

The example above uses `printf` and a build-time float literal to
get started quickly on a desktop. On an embedded target neither is
usually available — there is no `<stdio.h>` and often no
floating-point unit. FR_Math ships three callback-based print
functions that replace `printf` for numeric output:

| Function | Output |
| --- | --- |
| `FR_printNumF(f, n, radix, pad, prec)` | Fixed-point value as decimal with a dot, e.g. `"3.14158"` |
| `FR_printNumD(f, n, pad)` | Plain signed integer, e.g. `"-42"` |
| `FR_printNumH(f, n, showPrefix)` | Hexadecimal, e.g. `"0x0003243f"` |

Each function takes a `int (*f)(char)` callback and calls it once
per output character. Point that callback at a UART transmit
register, an LCD driver, a ring buffer — whatever the platform
provides.

Save this as `hello_embedded.c`:

```c
/* hello_embedded.c — FR_Math on bare metal, no printf, no float. */
#include "FR_defs.h"
#include "FR_math.h"

/* ---- Platform glue ------------------------------------------------
 * Replace uart_putc() with your board's serial-TX function.
 * The FR_printNum family calls f(char) once per character.
 */
static int uart_putc(char c)
{
    /* e.g. STM32:  HAL_UART_Transmit(&huart1, (uint8_t *)&c, 1, 10); */
    /* e.g. AVR:    while (!(UCSR0A & (1 << UDRE0))); UDR0 = c;       */
    (void)c;
    return 0;
}

/* Helper: emit a C string through the same callback. */
static void emit(const char *s) { while (*s) uart_putc(*s++); }

int main(void)
{
    const int radix = 16;

    /* ---- Compile-time constant: FR_NUM(integer, frac, digits, radix) ----
     *
     * FR_NUM builds a fixed-point value from its decimal parts:
     *   arg 1 (i): integer part          — here 3
     *   arg 2 (f): fractional digits     — here 14159  (the digits after ".")
     *   arg 3 (d): how many digits in f  — here 5      (1-4-1-5-9 = 5 digits)
     *   arg 4 (r): radix (frac bits)     — here 16
     *
     * Result: (3 << 16) + (14159 << 16) / 100000 = 205887 = 0x0003243F
     *
     * Because every argument is a literal, the compiler folds the entire
     * expression to a single constant — zero runtime cost, no float.
     * Use FR_NUM for any value you know at compile time.              */
    s32 pi     = FR_NUM(3, 14159, 5, radix);   /* 3.14159  s15.16  */
    s32 two    = I2FR(2, radix);                /* 2.0      s15.16  */
    s32 two_pi = FR_FixMuls(pi, two, radix);    /* 2 * pi   s15.16  */

    /* ---- Runtime string parser: FR_numstr(string, radix) ----
     *
     * FR_numstr is the runtime counterpart of FR_NUM.  It parses a
     * null-terminated decimal string (like "3.14159") and returns the
     * same fixed-point value that FR_NUM would produce.
     *
     * Use it when the number is not known at compile time — for
     * example, reading from a serial port, a config file, or user
     * input on an LCD menu:
     *
     *   s32 gain = FR_numstr(uart_line_buf, 16);   // e.g. "0.05"
     *   s32 freq = FR_numstr(cfg_lookup("freq"), 8);
     *
     * It handles signs ("-3.5"), leading whitespace ("  3.14"),
     * leading-zero fractions ("0.05"), and up to 9 decimal digits.
     * No malloc, no strtod, no libm — safe for bare-metal use.
     *
     * FR_numstr is the inverse of FR_printNumF: one converts a
     * fixed-point number to a string, the other converts it back.    */

    /* FR_printNumF(putc, value, radix, min_width, frac_digits)
     * Prints a string like "3.14158" through the callback.           */
    FR_printNumF(uart_putc, pi,     radix, 0, 5);
    emit(" * 2 = ");
    FR_printNumF(uart_putc, two_pi, radix, 0, 5);
    emit("\r\n");

    /* Trig: cos(45 deg) printed as fixed-point and hex. */
    s16 cos45 = FR_CosI(45);                         /* s0.15         */
    emit("cos(45) = ");
    FR_printNumF(uart_putc, (s32)cos45, 15, 0, 5);   /* decimal       */
    emit(" (");
    FR_printNumH(uart_putc, cos45, 1);                /* hex           */
    emit(")\r\n");

    /* Plain integer output with FR_printNumD. */
    emit("raw = ");
    FR_printNumH(uart_putc, (int)pi, 1);
    emit(" radix=");
    FR_printNumD(uart_putc, radix, 0);
    emit("\r\n");

    while (1) {}   /* bare-metal main never returns */
}
```

Compile for a Cortex-M4 (or any bare-metal C99 target):

```bash
arm-none-eabi-gcc -Isrc -mcpu=cortex-m4 -mthumb -Os \
    hello_embedded.c src/FR_math.c --specs=nosys.specs -o hello_embedded.elf
```

Expected serial output (32-bit `int` platform):

```
3.14158 * 2 = 6.28317
cos(45) = 0.70709 (0x00005a82)
raw = 0x0003243f radix=16
```

Key differences from the desktop version:

- **No float anywhere.** `FR_NUM(3, 14159, 5, 16)` builds the
  constant from the integer 3, the five fractional digits 14159, and
  the radix 16. The compiler folds the whole expression to a single
  `0x0003243f` literal at compile time. For runtime parsing (e.g.
  from a serial port or config file), use `FR_numstr("3.14159", 16)`.
- **No `<stdio.h>`.** All output goes through the single-character
  callback. The three print functions together add under 500 bytes
  of code on Cortex-M4 at `-Os`.
- **Buffer variant.** To print into RAM instead of a UART, swap
  `uart_putc` for a function that appends to a `char[]` and bumps
  a write pointer:

```c
static char buf[64];
static int  pos = 0;

static int buf_putc(char c) { buf[pos++] = c; return 0; }
```

Then pass `buf_putc` in place of `uart_putc`. After the call,
`buf[0..pos-1]` holds the formatted string.

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

As of v2.0.0, FR_Math ships with 42 passing tests and 99% line
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
