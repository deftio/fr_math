/**
 * fixed_point_basics.cpp — Educational walkthrough of fixed-point fundamentals
 *
 * Demonstrates: radix interpretation, I2FR/FR2I conversions, FR_NUM construction,
 * add/sub alignment, multiply precision, division, saturation, and FR_printNumF.
 *
 * Build:  make ex_basics
 * Run:    ./build/ex_basics
 *
 * Copyright (C) 2001-2026 M. A. Chatterjee — zlib license (see FR_math.h)
 */

#include <stdio.h>
#include <math.h>

#include "FR_defs.h"
#include "FR_math.h"

/* putchar callback for FR_printNumF */
static int put_char(char c) { return putchar(c); }

/* ------------------------------------------------------------------ */
static void section(const char *label)
{
    printf("\n========================================\n");
    printf("  %s\n", label);
    printf("========================================\n\n");
}

/* ================================================================== */
int main()
{
    printf("FR_Math — Fixed-Point Basics  (v%s)\n", FR_MATH_VERSION);

    /* -------------------------------------------------------------- */
    /* A. Same integer at different radixes                            */
    /* -------------------------------------------------------------- */
    section("A. Same integer at radixes 0-15");

    s32 val = 1234;
    printf("  Raw integer value: %d\n\n", (int)val);
    printf("  radix  float-equiv    precision (1/2^r)\n");
    printf("  -----  -----------    -------------------\n");
    for (int r = 0; r <= 15; r++) {
        printf("  %5d  %11.5f    1/%-5d = %.8f\n",
               r, FR2D(val, r), 1 << r, FR2D(1, r));
    }

    /* -------------------------------------------------------------- */
    /* B. I2FR / FR2I round-trip conversions                          */
    /* -------------------------------------------------------------- */
    section("B. I2FR / FR2I round-trip conversions");

    int test_ints[] = {0, 1, -1, 42, -100, 32767};
    int n_tests = (int)(sizeof(test_ints) / sizeof(test_ints[0]));
    int rdx = 12;
    printf("  Radix = %d\n\n", rdx);
    printf("  int   ->  I2FR(int,%d)  ->  FR2I(fr,%d)  ->  float-equiv\n", rdx, rdx);
    printf("  ----      -----------       -----------       -----------\n");
    for (int i = 0; i < n_tests; i++) {
        s32 fr = I2FR(test_ints[i], rdx);
        s32 back = FR2I(fr, rdx);
        printf("  %6d    %11d       %11d       %11.4f\n",
               test_ints[i], (int)fr, (int)back, FR2D(fr, rdx));
    }

    /* -------------------------------------------------------------- */
    /* C. FR_NUM — construct pi (3.14159) at radix 12                 */
    /* -------------------------------------------------------------- */
    section("C. FR_NUM — construct fractional constants");

    rdx = 12;
    s32 pi_fr = FR_NUM(3, 14159, 5, rdx);
    s32 neg_half = FR_NUM(0, 5, 1, rdx);
    neg_half = -neg_half; /* -0.5 */
    printf("  pi at radix %d:   raw = %d,  float = %.6f  (ref %.6f)\n",
           rdx, (int)pi_fr, FR2D(pi_fr, rdx), 3.14159);
    printf("  -0.5 at radix %d: raw = %d,  float = %.6f\n",
           rdx, (int)neg_half, FR2D(neg_half, rdx));

    rdx = 16;
    pi_fr = FR_NUM(3, 14159, 5, rdx);
    printf("  pi at radix %d:  raw = %d,  float = %.6f  (ref %.6f)\n",
           rdx, (int)pi_fr, FR2D(pi_fr, rdx), 3.14159);

    /* -------------------------------------------------------------- */
    /* D. Add/sub with aligned radix                                  */
    /* -------------------------------------------------------------- */
    section("D. Addition / subtraction with radix alignment");

    {
        int ra = 10, rb = 10;
        s32 a = FR_NUM(2, 5, 1, ra);   /* 2.5 at radix 10 */
        s32 b = FR_NUM(1, 25, 2, rb);  /* 1.25 at radix 10 */
        s32 sum = a + b;               /* same radix — direct add */
        printf("  Same radix (%d):\n", ra);
        printf("    2.5 + 1.25 = %.4f  (raw: %d + %d = %d)\n",
               FR2D(sum, ra), (int)a, (int)b, (int)sum);

        /* Different radixes — must align first */
        int ra2 = 8, rb2 = 12;
        s32 a2 = FR_NUM(2, 5, 1, ra2);   /* 2.5 at radix 8 */
        s32 b2 = FR_NUM(1, 25, 2, rb2);  /* 1.25 at radix 12 */

        /* Wrong: adding directly without alignment */
        s32 wrong = a2 + b2;
        printf("\n  Different radixes (a=r%d, b=r%d):\n", ra2, rb2);
        printf("    WRONG (no align):   raw %d + %d = %d  (%.4f ?""?)\n",
               (int)a2, (int)b2, (int)wrong, FR2D(wrong, ra2));

        /* Right: use FR_ADD which aligns b to a's radix */
        s32 a2_copy = a2;
        FR_ADD(a2_copy, ra2, b2, rb2);
        printf("    RIGHT (FR_ADD):     result raw = %d  (%.4f)\n",
               (int)a2_copy, FR2D(a2_copy, ra2));
    }

    /* -------------------------------------------------------------- */
    /* E. Multiply — precision doubling and truncation                */
    /* -------------------------------------------------------------- */
    section("E. Multiply — precision doubling, FR_CHRDX");

    {
        int r = 12;
        s32 a = FR_NUM(3, 5, 1, r);    /* 3.5  at radix 12 */
        s32 b = FR_NUM(2, 25, 2, r);   /* 2.25 at radix 12 */
        s32 product = a * b;            /* result is at radix 24 (12+12) */
        printf("  3.5 * 2.25 at radix %d:\n", r);
        printf("    a = %d (%.4f), b = %d (%.4f)\n",
               (int)a, FR2D(a, r), (int)b, FR2D(b, r));
        printf("    product raw = %d  at radix %d  (%.6f)\n",
               (int)product, 2 * r, FR2D(product, 2 * r));
        printf("    ref = %.6f\n", 3.5 * 2.25);

        /* Truncate back to original radix */
        s32 truncated = FR_CHRDX(product, 2 * r, r);
        printf("    FR_CHRDX to radix %d: %d (%.4f)\n",
               r, (int)truncated, FR2D(truncated, r));
    }

    /* -------------------------------------------------------------- */
    /* F. Division                                                    */
    /* -------------------------------------------------------------- */
    section("F. Division (FR_DIV, FR_DIV32)");

    {
        int r = 16;
        s32 a = I2FR(7, r);
        s32 b = I2FR(3, r);
        s32 q64 = FR_DIV(a, r, b, r);    /* 64-bit intermediate, rounded */
        printf("  7 / 3 at radix %d:\n", r);
        printf("    FR_DIV  (64-bit, rounded):   %d  (%.6f)\n",
               (int)q64, FR2D(q64, r));
        printf("    ref = %.6f\n", 7.0 / 3.0);

        /* FR_DIV32 works when x << yr fits in s32.
         * At radix 8, x=7 → x<<8 = 1792, well within range. */
        int r8 = 8;
        a = I2FR(7, r8);
        b = I2FR(3, r8);
        s32 q32 = FR_DIV32(a, r8, b, r8);
        printf("\n  7 / 3 at radix %d (FR_DIV32, 32-bit only):\n", r8);
        printf("    FR_DIV32: %d  (%.6f)   ref = %.6f\n",
               (int)q32, FR2D(q32, r8), 7.0 / 3.0);

        a = I2FR(22, r);
        b = I2FR(7, r);
        q64 = FR_DIV(a, r, b, r);
        printf("\n  22 / 7 at radix %d:\n", r);
        printf("    FR_DIV:  %d  (%.6f)   ref = %.6f\n",
               (int)q64, FR2D(q64, r), 22.0 / 7.0);
    }

    /* -------------------------------------------------------------- */
    /* G. Saturation — overflow vs saturate                           */
    /* -------------------------------------------------------------- */
    section("G. Saturation (FR_FixAddSat, FR_FixMulSat)");

    {
        s32 big = 0x70000000;
        s32 also_big = 0x20000000;
        s32 raw_add = big + also_big;           /* overflows! */
        s32 sat_add = FR_FixAddSat(big, also_big);

        printf("  Addition overflow:\n");
        printf("    0x%08X + 0x%08X\n", (unsigned)big, (unsigned)also_big);
        printf("    raw add:       0x%08X  (%d) — OVERFLOW!\n",
               (unsigned)raw_add, (int)raw_add);
        printf("    FR_FixAddSat:  0x%08X  (%d) — saturated\n",
               (unsigned)sat_add, (int)sat_add);

        s32 x = 50000;
        s32 y = 50000;
        s32 raw_mul = x * y;                     /* overflows at 32-bit */
        s32 sat_mul = FR_FixMulSat(x, y);

        printf("\n  Multiply overflow (as s15.16):\n");
        printf("    %d * %d\n", (int)x, (int)y);
        printf("    raw mul:       %d — OVERFLOW!\n", (int)raw_mul);
        printf("    FR_FixMulSat:  %d  (%.4f as s15.16) — saturated\n",
               (int)sat_mul, FR2D(sat_mul, 16));
    }

    /* -------------------------------------------------------------- */
    /* H. FR_printNumF formatted output                               */
    /* -------------------------------------------------------------- */
    section("H. FR_printNumF — formatted fixed-point printing");

    {
        int r = 13;
        s32 z  = (s32)(123.456 * (1 << r));
        s32 zn = -z;

        printf("  z  = %d (raw), float = %.4f\n", (int)z, FR2D(z, r));
        printf("  zn = %d (raw), float = %.4f\n\n", (int)zn, FR2D(zn, r));

        printf("  FR_printNumF(z,  r=%d, pad=6, prec=3): ", r);
        FR_printNumF(put_char, z, r, 6, 3);
        printf("\n");

        printf("  FR_printNumF(zn, r=%d, pad=6, prec=3): ", r);
        FR_printNumF(put_char, zn, r, 6, 3);
        printf("\n");

        printf("  FR_printNumF(z,  r=%d, pad=8, prec=5): ", r);
        FR_printNumF(put_char, z, r, 8, 5);
        printf("\n");

        printf("  FR_printNumF(zn, r=%d, pad=8, prec=5): ", r);
        FR_printNumF(put_char, zn, r, 8, 5);
        printf("\n");
    }

    printf("\n--- end ---\n");
    return 0;
}
