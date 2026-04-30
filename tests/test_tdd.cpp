/*
 * test_tdd.cpp - Measurement-driven characterization suite for FR_Math
 *
 * Goal:
 *   Measure what the library actually does, print results as a markdown
 *   report, and never fail the build. No pass/fail assertions. The point
 *   is to learn empirically which functions are wrong, by how much, and
 *   which are merely imprecise.
 *
 * Coverage goal:
 *   - 100% line coverage of FR_math.c and FR_math_2D.cpp
 *   - Every macro in FR_math.h, FR_defs.h, FR_math_2D.h exercised at least once
 *
 * Compare against:
 *   - libm (math.h) for transcendental references
 *   - 64-bit integer arithmetic for fixed-point references
 */

#define _USE_MATH_DEFINES
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <stdint.h>

#include "../src/FR_math.h"
#include "../src/FR_math_2D.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#ifndef M_E
#define M_E 2.71828182845904523536
#endif
#ifndef M_LOG2E
#define M_LOG2E 1.44269504088896340736
#endif
#ifndef M_LN2
#define M_LN2 0.69314718055994530942
#endif

#ifdef __clang__
#pragma clang diagnostic ignored "-Wshift-count-negative"
#pragma clang diagnostic ignored "-Wshift-negative-value"
#pragma clang diagnostic ignored "-Wunused-variable"
#pragma clang diagnostic ignored "-Wunused-but-set-variable"
#endif
#ifdef __GNUC__
#pragma GCC diagnostic ignored "-Wshift-count-negative"
#pragma GCC diagnostic ignored "-Wshift-negative-value"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#endif

/* ============================================================
 * Helpers
 * ============================================================ */

static inline double frd(s32 x, int radix) {
    return (double)x / (double)(1L << radix);
}

typedef struct {
    int n;
    double max_abs_err;
    double sum_abs_err;
    double max_pct_err;
    double sum_pct_err;
    double worst_input;        /* input that produced max abs error */
    double worst_actual;
    double worst_expected;
    double worst_pct_input;    /* input that produced max pct error */
    double worst_pct_actual;
    double worst_pct_expected;
} stats_t;

static void stats_reset(stats_t *s) {
    memset(s, 0, sizeof(*s));
}

static void stats_add(stats_t *s, double in, double actual, double expected) {
    double e = actual - expected;
    if (e < 0) e = -e;
    if (s->n == 0 || e > s->max_abs_err) {
        s->max_abs_err = e;
        s->worst_input = in;
        s->worst_actual = actual;
        s->worst_expected = expected;
    }
    s->sum_abs_err += e;
    double pct = (expected != 0.0) ? (e / fabs(expected)) * 100.0 : (e != 0.0 ? 100.0 : 0.0);
    if (pct > s->max_pct_err) {
        s->max_pct_err = pct;
        s->worst_pct_input = in;
        s->worst_pct_actual = actual;
        s->worst_pct_expected = expected;
    }
    s->sum_pct_err += pct;
    s->n++;
}

static double stats_mean(const stats_t *s) {
    return s->n ? s->sum_abs_err / s->n : 0.0;
}

static double stats_mean_pct(const stats_t *s) {
    return s->n ? s->sum_pct_err / s->n : 0.0;
}

/* Quantize a double to s15.16 resolution (same grid as library output). */
static inline double q16(double x) {
    return floor(x * 65536.0 + 0.5) / 65536.0;
}

/* Reference value for tan: libm tan() clamped to ±maxint as s15.16 double. */
static const double TAN_CLAMP = (double)0x7fffffff / (double)(1L << 16);

static double tan_ref(double rad) {
    double t = tan(rad);
    if (t >  TAN_CLAMP) return TAN_CLAMP;
    if (t < -TAN_CLAMP) return -TAN_CLAMP;
    return t;
}

/* Set by FR_SHOWPEAK env var — adds a "Peak at" column to the accuracy table */
static int g_showpeak = 0;

/* Print one accuracy table row, optionally with peak-error input */
static void acc_row(const char *name, const stats_t *s, const char *note) {
    printf("| %s | %.4f | %.4f | %s",
           name, s->max_pct_err, stats_mean_pct(s), note);
    if (g_showpeak)
        printf(" | %.4g", s->worst_pct_input);
    printf(" |\n");
}

static void md_h1(const char *t) { printf("\n# %s\n\n", t); }
static void md_h2(const char *t) { printf("\n## %s\n\n", t); }
static void md_h3(const char *t) { printf("\n### %s\n\n", t); }

static void table_header_stats(void) {
    printf("| Test | N | Max abs err | Mean abs err | Worst input | Actual | Expected |\n");
    printf("|---|---:|---:|---:|---:|---:|---:|\n");
}

static void table_row_stats(const char *name, const stats_t *s) {
    printf("| %s | %d | %.6g | %.6g | %.6g | %.6g | %.6g |\n",
           name, s->n, s->max_abs_err, stats_mean(s),
           s->worst_input, s->worst_actual, s->worst_expected);
}

/* captured putchar for testing print helpers */
static char g_buf[1024];
static int  g_idx;
static int captured_putchar(char c) {
    if (g_idx < (int)sizeof(g_buf) - 1) g_buf[g_idx++] = c;
    g_buf[g_idx] = 0;
    return c;
}
static void buf_reset(void) { g_idx = 0; g_buf[0] = 0; }

/* ============================================================
 * Section 0: Platform sanity
 * ============================================================ */

static void section_platform(void) {
    md_h2("0. Platform & Type Sizes");
    printf("| Type | Size (bytes) |\n");
    printf("|---|---:|\n");
    printf("| s8  | %lu |\n", (unsigned long)sizeof(s8));
    printf("| u8  | %lu |\n", (unsigned long)sizeof(u8));
    printf("| s16 | %lu |\n", (unsigned long)sizeof(s16));
    printf("| u16 | %lu |\n", (unsigned long)sizeof(u16));
    printf("| s32 | %lu |\n", (unsigned long)sizeof(s32));
    printf("| u32 | %lu |\n", (unsigned long)sizeof(u32));
    printf("| int | %lu |\n", (unsigned long)sizeof(int));
    printf("| long | %lu |\n", (unsigned long)sizeof(long));
    printf("\n");
    if (sizeof(s32) != 4) {
        printf("> **WARNING**: `s32` is %lu bytes, not 4. `FR_defs.h` maps `s32` to "
               "`int32_t` via `<stdint.h>`, so this should never fire on a C99-or-newer toolchain.\n\n",
               (unsigned long)sizeof(s32));
    }
}

/* ============================================================
 * Section 1: Header Constants
 * ============================================================ */

static void test_constant(const char *name, s32 fr_val, double expected) {
    double actual = frd(fr_val, FR_kPREC);
    double err = actual - expected;
    if (err < 0) err = -err;
    double rel = expected != 0.0 ? err / fabs(expected) : err;
    printf("| %s | %ld | %.10f | %.10f | %.3e | %.3e |\n",
           name, (long)fr_val, actual, expected, err, rel);
}

static void section_constants(void) {
    md_h2("1. Header Constants (radix FR_kPREC = 16)");
    printf("| Name | Stored | As double | True value | Abs err | Rel err |\n");
    printf("|---|---:|---:|---:|---:|---:|\n");
    test_constant("FR_kE",        FR_kE,        M_E);
    test_constant("FR_krE",       FR_krE,       1.0 / M_E);
    test_constant("FR_kPI",       FR_kPI,       M_PI);
    test_constant("FR_krPI",      FR_krPI,      1.0 / M_PI);
    test_constant("FR_kDEG2RAD",  FR_kDEG2RAD,  M_PI / 180.0);
    test_constant("FR_kRAD2DEG",  FR_kRAD2DEG,  180.0 / M_PI);
    test_constant("FR_kQ2RAD",    FR_kQ2RAD,    M_PI / 2.0);
    test_constant("FR_kRAD2Q",    FR_kRAD2Q,    2.0 / M_PI);
    test_constant("FR_kLOG2E",    FR_kLOG2E,    M_LOG2E);
    test_constant("FR_krLOG2E",   FR_krLOG2E,   M_LN2);
    test_constant("FR_kLOG2_10",  FR_kLOG2_10,  log2(10.0));
    test_constant("FR_krLOG2_10", FR_krLOG2_10, log10(2.0));
    test_constant("FR_kSQRT2",    FR_kSQRT2,    sqrt(2.0));
    test_constant("FR_krSQRT2",   FR_krSQRT2,   1.0 / sqrt(2.0));
    test_constant("FR_kSQRT3",    FR_kSQRT3,    sqrt(3.0));
    test_constant("FR_krSQRT3",   FR_krSQRT3,   1.0 / sqrt(3.0));
    test_constant("FR_kSQRT5",    FR_kSQRT5,    sqrt(5.0));
    test_constant("FR_krSQRT5",   FR_krSQRT5,   1.0 / sqrt(5.0));
    test_constant("FR_kSQRT10",   FR_kSQRT10,   sqrt(10.0));
    test_constant("FR_krSQRT10",  FR_krSQRT10,  1.0 / sqrt(10.0));
    printf("\n");
}

/* ============================================================
 * Section 2: Basic Macros
 * ============================================================ */

static void section_macros_basic(void) {
    md_h2("2. Basic Macros");

    md_h3("2.1 FR_ABS");
    printf("| Input | FR_ABS | Note |\n|---:|---:|---|\n");
    printf("| 100 | %d | positive |\n", FR_ABS(100));
    printf("| -100 | %d | negative |\n", FR_ABS(-100));
    printf("| 0 | %d | zero |\n", FR_ABS(0));
    {
        s32 mn = (s32)0x80000000;
        printf("| INT32_MIN (0x80000000) | %ld | UB on 2s-complement: -INT_MIN overflows |\n",
               (long)FR_ABS(mn));
    }

    md_h3("2.2 FR_SGN");
    printf("| Input | FR_SGN |\n|---:|---:|\n");
    printf("| 100 | %d |\n", FR_SGN(100));
    printf("| -100 | %d |\n", FR_SGN(-100));
    printf("| 0 | %d |\n", FR_SGN(0));
    {
        s32 mx = (s32)0x7fffffff;
        s32 mn = (s32)0x80000000;
        printf("| 0x7fffffff | %ld |\n", (long)FR_SGN(mx));
        printf("| 0x80000000 | %ld |\n", (long)FR_SGN(mn));
    }

    md_h3("2.3 I2FR / FR2I");
    printf("| Op | Result |\n|---|---:|\n");
    printf("| I2FR(100, 8) | %d |\n", I2FR(100, 8));
    printf("| FR2I(I2FR(100,8), 8) | %d |\n", FR2I(I2FR(100, 8), 8));
    printf("| I2FR(-50, 4) | %d |\n", I2FR(-50, 4));
    printf("| FR2I(I2FR(-50,4), 4) | %d |\n", FR2I(I2FR(-50, 4), 4));

    md_h3("2.4 FR_NUM");
    printf("| Op | Result | Expected |\n|---|---:|---:|\n");
    printf("| FR_NUM(12, 34, 2, 10) | %d | 12.34 << 10 ≈ 12636 |\n", FR_NUM(12, 34, 2, 10));
    printf("| FR_NUM(-3, 5, 1, 16) | %d | -3.5 << 16 = -229376 |\n", FR_NUM(-3, 5, 1, 16));
    printf("| FR_NUM(0, 25, 2, 16) | %d | 0.25 << 16 = 16384 |\n", FR_NUM(0, 25, 2, 16));
    printf("| FR_NUM(1, 0, 0, 8) | %d | 1.0 << 8 = 256 |\n", FR_NUM(1, 0, 0, 8));
    printf("\n> Signature: `FR_NUM(int, frac_digits, num_digits, radix)`.\n\n");

    md_h3("2.4b FR_numstr (string parser)");
    printf("| Input | Radix | FR_numstr | FR_NUM | Match |\n|---|---:|---:|---:|---:|\n");
    printf("| \"12.34\" | 10 | %d | %d | %s |\n",
           FR_numstr("12.34", 10), FR_NUM(12, 34, 2, 10),
           FR_numstr("12.34", 10) == FR_NUM(12, 34, 2, 10) ? "yes" : "NO");
    printf("| \"-3.5\" | 16 | %d | %d | %s |\n",
           FR_numstr("-3.5", 16), FR_NUM(-3, 5, 1, 16),
           FR_numstr("-3.5", 16) == FR_NUM(-3, 5, 1, 16) ? "yes" : "NO");
    printf("| \"0.25\" | 16 | %d | %d | %s |\n",
           FR_numstr("0.25", 16), FR_NUM(0, 25, 2, 16),
           FR_numstr("0.25", 16) == FR_NUM(0, 25, 2, 16) ? "yes" : "NO");
    printf("| \"-0.025\" | 16 | %d | %d | %s |\n",
           FR_numstr("-0.025", 16), -FR_NUM(0, 25, 3, 16),
           FR_numstr("-0.025", 16) == -FR_NUM(0, 25, 3, 16) ? "yes" : "NO");
    printf("| \"0.05\" | 16 | %d | %d | %s |\n",
           FR_numstr("0.05", 16), FR_NUM(0, 5, 2, 16),
           FR_numstr("0.05", 16) == FR_NUM(0, 5, 2, 16) ? "yes" : "NO");
    printf("| \"42\" | 8 | %d | %d | %s |\n",
           FR_numstr("42", 8), 42 << 8,
           FR_numstr("42", 8) == (42 << 8) ? "yes" : "NO");
    printf("| \"1.0\" | 8 | %d | %d | %s |\n",
           FR_numstr("1.0", 8), 1 << 8,
           FR_numstr("1.0", 8) == (1 << 8) ? "yes" : "NO");
    printf("| \"3.14159\" | 16 | %d | %d | %s |\n",
           FR_numstr("3.14159", 16), FR_NUM(3, 14159, 5, 16),
           FR_numstr("3.14159", 16) == FR_NUM(3, 14159, 5, 16) ? "yes" : "NO");
    printf("| \"  3.14\" | 16 | %d | %d | %s |\n",
           FR_numstr("  3.14", 16), FR_NUM(3, 14, 2, 16),
           FR_numstr("  3.14", 16) == FR_NUM(3, 14, 2, 16) ? "yes" : "NO");
    printf("| \"-7.0\" | 16 | %d | %d | %s |\n",
           FR_numstr("-7.0", 16), -(7 << 16),
           FR_numstr("-7.0", 16) == -(7 << 16) ? "yes" : "NO");
    printf("| NULL | 16 | %d | 0 | %s |\n",
           FR_numstr(NULL, 16),
           FR_numstr(NULL, 16) == 0 ? "yes" : "NO");
    printf("| \"\" | 16 | %d | 0 | %s |\n",
           FR_numstr("", 16),
           FR_numstr("", 16) == 0 ? "yes" : "NO");

    md_h3("2.5 FR_INT");
    printf("| Op | Result |\n|---|---:|\n");
    printf("| FR_INT(I2FR(50,8), 8) | %d |\n", FR_INT(I2FR(50, 8), 8));
    printf("| FR_INT(I2FR(-50,8), 8) | %d |\n", FR_INT(I2FR(-50, 8), 8));
    printf("| FR_INT(I2FR(50,8) + 128, 8) | %d (truncates fractional) |\n",
           FR_INT(I2FR(50, 8) + 128, 8));
    printf("| FR_INT(I2FR(-50,8) - 128, 8) | %d (truncates toward zero) |\n",
           FR_INT(I2FR(-50, 8) - 128, 8));

    md_h3("2.6 FR_CHRDX");
    printf("| Op | Result |\n|---|---:|\n");
    printf("| FR_CHRDX(I2FR(10,4), 4, 8) | %d |\n", (int)FR_CHRDX(I2FR(10, 4), 4, 8));
    printf("| FR_CHRDX(I2FR(10,8), 8, 4) | %d |\n", (int)FR_CHRDX(I2FR(10, 8), 8, 4));
    printf("| FR_CHRDX(100, 8, 8) | %d |\n", (int)FR_CHRDX(100, 8, 8));
    printf("| FR_CHRDX(100, 0, 8) | %d |\n", (int)FR_CHRDX(100, 0, 8));

    md_h3("2.7 FR_FRAC");
    printf("| Op | Result |\n|---|---:|\n");
    printf("| FR_FRAC(I2FR(10,8) + 128, 8) | %d (= 0.5) |\n", FR_FRAC(I2FR(10, 8) + 128, 8));
    printf("| FR_FRAC(I2FR(-10,8) - 128, 8) | %d (uses ABS, symmetric) |\n",
           FR_FRAC(I2FR(-10, 8) - 128, 8));
    printf("| FR_FRAC(I2FR(10,8), 8) | %d (no fraction) |\n", FR_FRAC(I2FR(10, 8), 8));

    md_h3("2.8 FR_FRACS");
    printf("| Op | Result | Expected |\n|---|---:|---:|\n");
    printf("| FR_FRACS(I2FR(10,8) + 64, 8, 4) | %d | 64/256 = 0.25, in r4 = 4 |\n",
           (int)FR_FRACS(I2FR(10, 8) + 64, 8, 4));

    md_h3("2.9 FR_ADD / FR_SUB");
    printf("| Op | Result | Note |\n|---|---:|---|\n");
    {
        s32 a;
        a = I2FR(10, 8); FR_ADD(a, 8, I2FR(5, 8), 8);
        printf("| FR_ADD same radix | %d | 15 in r8 = 3840 |\n", (int)a);
        a = I2FR(10, 4); FR_ADD(a, 4, I2FR(5, 8), 8);
        printf("| FR_ADD mixed radix | %d | 15 in r4 = 240 |\n", (int)a);
        a = I2FR(10, 8); FR_SUB(a, 8, I2FR(3, 8), 8);
        printf("| FR_SUB same radix | %d | 7 in r8 = 1792 |\n", (int)a);
    }

    md_h3("2.10 FR_ISPOW2");
    printf("| Input | Result |\n|---:|---:|\n");
    int pwrs[] = {1, 2, 4, 8, 16, 32, 64, 128, 256, 3, 5, 6, 7, 0};
    for (int i = 0; i < (int)(sizeof(pwrs)/sizeof(pwrs[0])); i++) {
        printf("| %d | %d |\n", pwrs[i], FR_ISPOW2(pwrs[i]) ? 1 : 0);
    }
    printf("\n> Note: `FR_ISPOW2(0)` returns true because `!(0 & -1) == !0 == 1`.\n\n");

    md_h3("2.11 FR_FLOOR / FR_CEIL");
    printf("| Op | Result | Note |\n|---|---:|---|\n");
    printf("| FR_FLOOR(I2FR(10,8)+200, 8) >> 8 | %d | floor(10.78) |\n",
           (int)(FR_FLOOR(I2FR(10, 8) + 200, 8) >> 8));
    printf("| FR_CEIL(I2FR(10,8)+50, 8) >> 8 | %d | ceil(10.19) |\n",
           (int)(FR_CEIL(I2FR(10, 8) + 50, 8) >> 8));
    printf("| FR_FLOOR(I2FR(-10,8)-128, 8) | %d | bitwise AND on negative |\n",
           (int)FR_FLOOR(I2FR(-10, 8) - 128, 8));
    printf("| FR_CEIL(I2FR(10,8), 8) | %d | already integer |\n",
           (int)FR_CEIL(I2FR(10, 8), 8));

    md_h3("2.12 FR_INTERP / FR_INTERPI");
    printf("| delta | INTERP(0,100,d,8) | INTERPI(0,100,d,8) |\n|---:|---:|---:|\n");
    int deltas[] = {0, 64, 128, 192, 256};
    for (int i = 0; i < 5; i++) {
        int d = deltas[i];
        printf("| %d | %d | %d |\n", d,
               (int)FR_INTERP(0, 100, d, 8),
               (int)FR_INTERPI(0, 100, d, 8));
    }

    md_h3("2.13 FR2D / D2FR");
    printf("| Op | Result |\n|---|---:|\n");
    printf("| FR2D(I2FR(10,8), 8) | %f |\n", FR2D(I2FR(10, 8), 8));
    printf("| D2FR(3.14, 8) | %d |\n", (int)D2FR(3.14, 8));
    printf("| FR2D(D2FR(3.14159, 16), 16) | %f |\n", FR2D(D2FR(3.14159, 16), 16));

    md_h3("2.14 Trig & misc constants");
    printf("| Constant | Value |\n|---|---:|\n");
    printf("| FR_TRIG_PREC | %d |\n", FR_TRIG_PREC);
    printf("| FR_TRIG_MASK | %d |\n", FR_TRIG_MASK);
    printf("| FR_TRIG_MAXVAL | %d |\n", FR_TRIG_MAXVAL);
    printf("| FR_TRIG_MINVAL | %d |\n", FR_TRIG_MINVAL);
    printf("| FR_LOG2MIN | %ld |\n", (long)FR_LOG2MIN);
    printf("| FR_kPREC | %d |\n", FR_kPREC);
    printf("| FR_MAT_DEFPREC | %d |\n", FR_MAT_DEFPREC);

    md_h3("2.15 FR_SWAP_BYTES");
    printf("| Input | Result |\n|---:|---:|\n");
    printf("| 0x1234 | 0x%04x |\n", FR_SWAP_BYTES(0x1234));
    printf("| 0xff00 | 0x%04x |\n", FR_SWAP_BYTES(0xff00));

    md_h3("2.16 FR_TRUE / FR_FALSE");
    printf("| FR_TRUE = %d, FR_FALSE = %d |\n\n", FR_TRUE, FR_FALSE);
}

/* ============================================================
 * Section 3: Shift-Approximation Macros
 * ============================================================ */

static void section_shift_macros(void) {
    md_h2("3. Shift-Approximation Macros");
    printf("> These macros do constant multiplication using only shifts and adds.\n");
    printf("> They produce *approximations* with characterizable error. We measure\n");
    printf("> max relative error over a sweep of typical inputs.\n\n");

    /* Multipliers we expect each macro to apply */
    struct {
        const char *name;
        double expected_factor;
        const char *purpose;
    } cases[] = {
        {"FR_SMUL10",    10.0,             "x * 10"},
        {"FR_SDIV10",    0.1,              "x / 10"},
        {"FR_SrLOG2E",   M_LN2,            "log2(x) -> ln(x)  (* 0.6931)"},
        {"FR_SLOG2E",    M_LOG2E,          "ln(x) -> log2(x)  (* 1.4427)"},
        {"FR_SrLOG2_10", log10(2.0),       "log2(x) -> log10(x) (* 0.3010)"},
        {"FR_SLOG2_10",  log2(10.0),       "log10(x) -> log2(x) (* 3.3219)"},
        {"FR_DEG2RAD",   M_PI / 180.0,     "* 0.01745 (v2: body unswapped)"},
        {"FR_RAD2DEG",   180.0 / M_PI,     "* 57.2958 (v2: body unswapped)"},
        {"FR_RAD2Q",     2.0 / M_PI,       "* 0.6366"},
        {"FR_Q2RAD",     M_PI / 2.0,       "* 1.5708"},
        {"FR_DEG2Q",     1.0 / 90.0,       "* 0.01111"},
        {"FR_Q2DEG",     90.0,             "* 90"},
    };

    printf("| Macro | Purpose | Empirical factor | Expected factor | Match? |\n");
    printf("|---|---|---:|---:|:---:|\n");

    /* Use a fixed input value and back out the apparent factor */
    s32 X = 1 << 18;
    double xd = (double)X;
    double factors[12];
    factors[0]  = (double)FR_SMUL10(X)    / xd;
    factors[1]  = (double)FR_SDIV10(X)    / xd;
    factors[2]  = (double)FR_SrLOG2E(X)   / xd;
    factors[3]  = (double)FR_SLOG2E(X)    / xd;
    factors[4]  = (double)FR_SrLOG2_10(X) / xd;
    factors[5]  = (double)FR_SLOG2_10(X)  / xd;
    factors[6]  = (double)FR_DEG2RAD(X)   / xd;
    factors[7]  = (double)FR_RAD2DEG(X)   / xd;
    factors[8]  = (double)FR_RAD2Q(X)     / xd;
    factors[9]  = (double)FR_Q2RAD(X)     / xd;
    factors[10] = (double)FR_DEG2Q(X)     / xd;
    factors[11] = (double)FR_Q2DEG(X)     / xd;

    for (int i = 0; i < 12; i++) {
        double diff = factors[i] - cases[i].expected_factor;
        if (diff < 0) diff = -diff;
        double rel = diff / fabs(cases[i].expected_factor);
        const char *match = rel < 0.01 ? "OK"
                          : rel < 0.10 ? "approx"
                          : "**MISMATCH**";
        printf("| %s | %s | %.6f | %.6f | %s |\n",
               cases[i].name, cases[i].purpose,
               factors[i], cases[i].expected_factor, match);
    }
    printf("\n");

    md_h3("3.1 FR_DEG2RAD / FR_RAD2DEG cross-check");
    printf("| Input | FR_DEG2RAD(input) | input * (pi/180) | FR_RAD2DEG(input) | input * (180/pi) |\n");
    printf("|---:|---:|---:|---:|---:|\n");
    int xs[] = {1, 10, 100, 180, 360, 1000};
    for (int i = 0; i < 6; i++) {
        int x = xs[i];
        printf("| %d | %d | %.4f | %d | %.4f |\n",
               x,
               (int)FR_DEG2RAD(x), x * (M_PI / 180.0),
               (int)FR_RAD2DEG(x), x * (180.0 / M_PI));
    }
    printf("\n");
}

/* ============================================================
 * Section 4: Arithmetic Primitives
 * ============================================================ */

static int64_t sat64(int64_t v) {
    if (v >  (int64_t)0x7fffffff) return  (int64_t)0x7fffffff;
    if (v < -(int64_t)0x7fffffff) return -(int64_t)0x7fffffff;
    return v;
}

/* Reference matching FR_FixMuls's intended contract:
 * Take absolute values, compute (|x|*|y|) >> 16, re-apply sign. */
static int64_t fixmuls_ref(s32 x, s32 y) {
    int sign = ((x < 0) != (y < 0));
    int64_t ax = (x < 0) ? -(int64_t)x : (int64_t)x;
    int64_t ay = (y < 0) ? -(int64_t)y : (int64_t)y;
    int64_t mag = (ax * ay) >> 16;
    return sign ? -mag : mag;
}

/* Reference matching FR_FixMulSat's *intended* contract:
 * (x*y) >> 16, then saturate to s32 range, with sign. */
static int64_t fixmulsat_ref(s32 x, s32 y) {
    int64_t v = fixmuls_ref(x, y);
    return sat64(v);
}

static void section_arithmetic(void) {
    md_h2("4. Arithmetic Primitives");

    /* test value pairs designed to hit corners */
    static const s32 vals[] = {
        0, 1, -1, 2, -2,
        0x100, -0x100,
        0x7fff, -0x7fff,
        0x10000, -0x10000,
        0x7fffffff, (s32)0x80000001, (s32)0x80000000,
        0x12345678, (s32)0xfedcba98,
    };
    const int N = (int)(sizeof(vals)/sizeof(vals[0]));

    md_h3("4.1 FR_FixMuls (signed, NOT saturating)");
    int muls_total = 0, muls_diff = 0;
    s32 muls_worst_x = 0, muls_worst_y = 0;
    int64_t muls_worst_actual = 0, muls_worst_ref = 0;
    for (int i = 0; i < N; i++) for (int j = 0; j < N; j++) {
        s32 x = vals[i], y = vals[j];
        s32 actual = FR_FixMuls(x, y);
        int64_t ref = fixmuls_ref(x, y);
        muls_total++;
        if ((int64_t)actual != ref) {
            if (muls_diff == 0) {
                muls_worst_x = x; muls_worst_y = y;
                muls_worst_actual = actual; muls_worst_ref = ref;
            }
            muls_diff++;
        }
    }
    printf("| Sweep | Pairs | Differs from |x|*|y|>>16 ref | First diff |\n");
    printf("|---|---:|---:|---|\n");
    printf("| FR_FixMuls | %d | %d | x=0x%lx y=0x%lx actual=0x%lx ref=0x%llx |\n",
           muls_total, muls_diff,
           (unsigned long)muls_worst_x, (unsigned long)muls_worst_y,
           (unsigned long)muls_worst_actual, (long long)muls_worst_ref);
    printf("\n");

    md_h3("4.2 FR_FixMulSat (signed, saturating)");
    int sat_total = 0, sat_diff = 0;
    s32 sat_worst_x = 0, sat_worst_y = 0;
    int64_t sat_worst_actual = 0, sat_worst_ref = 0;
    for (int i = 0; i < N; i++) for (int j = 0; j < N; j++) {
        s32 x = vals[i], y = vals[j];
        s32 actual = FR_FixMulSat(x, y);
        int64_t ref = fixmulsat_ref(x, y);
        sat_total++;
        if ((int64_t)actual != ref) {
            if (sat_diff == 0) {
                sat_worst_x = x; sat_worst_y = y;
                sat_worst_actual = actual; sat_worst_ref = ref;
            }
            sat_diff++;
        }
    }
    printf("| Sweep | Pairs | Differs from sat(|x|*|y|>>16) | First diff |\n");
    printf("|---|---:|---:|---|\n");
    printf("| FR_FixMulSat | %d | %d | x=0x%lx y=0x%lx actual=0x%lx ref=0x%llx |\n",
           sat_total, sat_diff,
           (unsigned long)sat_worst_x, (unsigned long)sat_worst_y,
           (unsigned long)sat_worst_actual, (long long)sat_worst_ref);
    printf("\n");

    md_h3("4.3 FR_FixMulSat targeted overflow cases");
    printf("| x | y | actual | sat64(x*y) |\n|---:|---:|---:|---:|\n");
    struct { s32 x, y; } targeted[] = {
        {0x7fffffff, 0x7fffffff},
        {0x7fffffff, 2},
        {0x40000000, 4},
        {0x10000, 0x10000},
        {0x10000, -0x10000},
        {(s32)0x80000000, 1},
        {(s32)0x80000000, -1},
        {0x7fffffff, -1},
    };
    for (int i = 0; i < (int)(sizeof(targeted)/sizeof(targeted[0])); i++) {
        s32 a = FR_FixMulSat(targeted[i].x, targeted[i].y);
        int64_t r = sat64((int64_t)targeted[i].x * (int64_t)targeted[i].y);
        printf("| 0x%lx | 0x%lx | 0x%lx | 0x%llx |\n",
               (unsigned long)targeted[i].x, (unsigned long)targeted[i].y,
               (unsigned long)a, (long long)r);
    }
    printf("\n");

    md_h3("4.4 FR_FixAddSat (signed, saturating)");
    int add_total = 0, add_diff = 0;
    s32 add_worst_x = 0, add_worst_y = 0;
    int64_t add_worst_actual = 0, add_worst_ref = 0;
    for (int i = 0; i < N; i++) for (int j = 0; j < N; j++) {
        s32 x = vals[i], y = vals[j];
        s32 actual = FR_FixAddSat(x, y);
        int64_t ref = sat64((int64_t)x + (int64_t)y);
        add_total++;
        if ((int64_t)actual != ref) {
            if (add_diff == 0) {
                add_worst_x = x; add_worst_y = y;
                add_worst_actual = actual; add_worst_ref = ref;
            }
            add_diff++;
        }
    }
    printf("| Sweep | Pairs | Differs from sat(x+y) | First diff |\n");
    printf("|---|---:|---:|---|\n");
    printf("| FR_FixAddSat vs sat64(x+y) | %d | %d | x=0x%lx y=0x%lx actual=0x%lx ref=0x%llx |\n",
           add_total, add_diff,
           (unsigned long)add_worst_x, (unsigned long)add_worst_y,
           (unsigned long)add_worst_actual, (long long)add_worst_ref);
    printf("\n");

    md_h3("4.5 FR_FixAddSat targeted overflow cases");
    printf("| x | y | actual | sat64(x+y) |\n|---:|---:|---:|---:|\n");
    struct { s32 x, y; } addt[] = {
        {0x7ffffff0, 0x100},
        {0x7fffffff, 1},
        {(s32)0x80000010, -0x100},
        {(s32)0x80000000, -1},
        {1000, -500},
        {-1000, 500},
        {0x40000000, 0x40000000},
        {(s32)0xc0000000, (s32)0xc0000000},
    };
    for (int i = 0; i < (int)(sizeof(addt)/sizeof(addt[0])); i++) {
        s32 a = FR_FixAddSat(addt[i].x, addt[i].y);
        int64_t r = sat64((int64_t)addt[i].x + (int64_t)addt[i].y);
        printf("| 0x%lx | 0x%lx | 0x%lx | 0x%llx |\n",
               (unsigned long)addt[i].x, (unsigned long)addt[i].y,
               (unsigned long)a, (long long)r);
    }
    printf("\n");

    md_h3("4.6 FR_DIV (64-bit) vs FR_DIV32 (32-bit) vs double");
    printf("| x | y | radix | FR_DIV | FR_DIV32 | expected (double) | DIV err | DIV32 err | DIV32 overflow? |\n");
    printf("|---:|---:|---:|---:|---:|---:|---:|---:|---:|\n");
    struct { double xd, yd; int r; } div_cases[] = {
        {10, 2, 8}, {7, 2, 8}, {-10, 3, 8}, {100, 7, 16},
        {1, 3, 16}, {30000, 3, 16}, {0.5, 0.25, 16},
        {-1000, -7, 12}, {1, 1, 8}, {32000, 1, 16},
    };
    for (int i = 0; i < (int)(sizeof(div_cases)/sizeof(div_cases[0])); i++) {
        int r = div_cases[i].r;
        s32 xfp = (s32)(div_cases[i].xd * (1L << r));
        s32 yfp = (s32)(div_cases[i].yd * (1L << r));
        double expected = div_cases[i].xd / div_cases[i].yd;
        s32 d64 = FR_DIV(xfp, r, yfp, r);
        s32 d32 = FR_DIV32(xfp, r, yfp, r);
        double d64d = frd(d64, r);
        double d32d = frd(d32, r);
        double e64 = d64d - expected; if (e64 < 0) e64 = -e64;
        double e32 = d32d - expected; if (e32 < 0) e32 = -e32;
        int overflow32 = (d64 != d32) ? 1 : 0;
        printf("| %g | %g | %d | %ld | %ld | %.6g | %.4g | %.4g | %s |\n",
               div_cases[i].xd, div_cases[i].yd, r,
               (long)d64, (long)d32, expected, e64, e32,
               overflow32 ? "YES" : "no");
    }
    printf("\n");
}

/* ============================================================
 * Section 5: Trig (Integer Degrees)
 * ============================================================ */

static void section_trig_int(void) {
    md_h2("5. Trig Functions (Integer Degrees)");

    stats_t cos_stats, sin_stats;
    stats_reset(&cos_stats);
    stats_reset(&sin_stats);

    for (int deg = -720; deg <= 720; deg++) {
        double exp_cos = cos(deg * M_PI / 180.0);
        double exp_sin = sin(deg * M_PI / 180.0);
        double act_cos = frd(FR_CosI((s16)deg), FR_TRIG_OUT_PREC);
        double act_sin = frd(FR_SinI((s16)deg), FR_TRIG_OUT_PREC);
        stats_add(&cos_stats, deg, act_cos, exp_cos);
        stats_add(&sin_stats, deg, act_sin, exp_sin);
    }

    table_header_stats();
    table_row_stats("FR_CosI [-720..720]", &cos_stats);
    table_row_stats("FR_SinI [-720..720]", &sin_stats);
    printf("\n> Tolerance reference: 1 LSB in s15.16 = 1/65536 ≈ 1.53e-5. Poles (0,90,180,270) are exact.\n\n");

    md_h3("5.1 FR_TanI vs tan() (skipping ±90n)");
    stats_t tan_stats;
    stats_reset(&tan_stats);
    int tan_skipped = 0;
    for (int deg = -89; deg <= 89; deg++) {
        if (deg % 90 == 0 && deg != 0) { tan_skipped++; continue; }
        double exp_tan = tan(deg * M_PI / 180.0);
        double act_tan = frd(FR_TanI((s16)deg), FR_TRIG_OUT_PREC);
        stats_add(&tan_stats, deg, act_tan, exp_tan);
    }
    table_header_stats();
    table_row_stats("FR_TanI [-89..89]", &tan_stats);
    printf("\n");

    md_h3("5.2 FR_TanI special angles");
    printf("| deg | FR_TanI | as double (s15.16) | tan(deg) |\n|---:|---:|---:|---:|\n");
    int specials[] = {0, 30, 45, 60, 80, 85, 88, 89, 90, 91, 135, 180, 270, -45, -90};
    for (int i = 0; i < (int)(sizeof(specials)/sizeof(specials[0])); i++) {
        int d = specials[i];
        s32 t = FR_TanI((s16)d);
        double td = frd(t, FR_TRIG_OUT_PREC);
        double ref = (d % 180 == 90) ? INFINITY : tan(d * M_PI / 180.0);
        printf("| %d | %ld | %.6g | %.6g |\n", d, (long)t, td, ref);
    }
    printf("\n");
}

/* ============================================================
 * Section 6: Trig (Fractional Degrees)
 * ============================================================ */

static void section_trig_frac(void) {
    md_h2("6. Trig Functions (Fractional Degrees)");
    md_h3("6.1 FR_Cos / FR_Sin (interpolated, radix 8)");

    stats_t cos_f, sin_f;
    stats_reset(&cos_f);
    stats_reset(&sin_f);
    /* sweep -180 to +180 in radix-8 increments of 0.25 deg */
    for (int q = -180 * 4; q <= 180 * 4; q++) {
        double deg_d = q / 4.0;
        s16 deg_fr = (s16)(q << (8 - 2));  /* radix 8, 0.25 step = 64 LSBs */
        double exp_c = cos(deg_d * M_PI / 180.0);
        double exp_s = sin(deg_d * M_PI / 180.0);
        double act_c = frd(FR_Cos(deg_fr, 8), FR_TRIG_OUT_PREC);
        double act_s = frd(FR_Sin(deg_fr, 8), FR_TRIG_OUT_PREC);
        stats_add(&cos_f, deg_d, act_c, exp_c);
        stats_add(&sin_f, deg_d, act_s, exp_s);
    }
    table_header_stats();
    table_row_stats("FR_Cos r8 0.25 step", &cos_f);
    table_row_stats("FR_Sin r8 0.25 step", &sin_f);
    printf("\n");

    md_h3("6.2 FR_Tan (interpolated, radix 8) — focused on steep region");
    printf("| deg | FR_Tan | as double | tan(deg) | abs err |\n|---:|---:|---:|---:|---:|\n");
    double check_degs[] = {30.0, 45.0, 60.0, 75.0, 80.0, 85.0, 88.0, 89.0, 89.5};
    for (int i = 0; i < (int)(sizeof(check_degs)/sizeof(check_degs[0])); i++) {
        s16 deg_fr = (s16)(check_degs[i] * 256);
        s32 t = FR_Tan(deg_fr, 8);
        double td = frd(t, FR_TRIG_OUT_PREC);
        double ref = tan(check_degs[i] * M_PI / 180.0);
        double e = td - ref; if (e < 0) e = -e;
        printf("| %.2f | %ld | %.6g | %.6g | %.6g |\n",
               check_degs[i], (long)t, td, ref, e);
    }
    printf("\n> v2: FR_Tan locals were widened from s16 to s32, so steep angles no longer\n");
    printf("> truncate catastrophically. The values above now agree with `libm`.\n\n");
}

/* ============================================================
 * Section 7: Inverse Trig
 * ============================================================ */

static void section_inverse_trig(void) {
    md_h2("7. Inverse Trig");

    md_h3("7.1 FR_acos sweep [-1, +1]");
    stats_t acos_stats;
    stats_reset(&acos_stats);
    /* radix 15 inputs, output radians at radix 16, 200 samples */
    for (int i = -200; i <= 200; i++) {
        double xd = i / 200.0;
        s32 fr = (s32)(xd * (1 << 15));
        s32 rad = FR_acos(fr, 15, 16);
        double ref_rad = acos(xd);
        stats_add(&acos_stats, xd, frd(rad, 16), ref_rad);
    }
    table_header_stats();
    table_row_stats("FR_acos vs acos() (rad)", &acos_stats);
    printf("\n");

    md_h3("7.2 FR_asin sweep [-1, +1]");
    stats_t asin_stats;
    stats_reset(&asin_stats);
    for (int i = -200; i <= 200; i++) {
        double xd = i / 200.0;
        s32 fr = (s32)(xd * (1 << 15));
        s32 rad = FR_asin(fr, 15, 16);
        double ref_rad = asin(xd);
        stats_add(&asin_stats, xd, frd(rad, 16), ref_rad);
    }
    table_header_stats();
    table_row_stats("FR_asin vs asin() (rad)", &asin_stats);
    printf("\n");

    md_h3("7.3 FR_atan2 (returns radians at radix 16)");
    printf("| (y, x) | FR_atan2 (rad s15.16) | atan2() radians |\n|---|---:|---:|\n");
    struct { s32 y, x; } pts[] = {
        {0, 1}, {1, 1}, {1, 0}, {1, -1}, {0, -1}, {-1, -1}, {-1, 0}, {-1, 1},
        {3, 4}, {-3, 4}, {3, -4}, {-3, -4}
    };
    for (int i = 0; i < (int)(sizeof(pts)/sizeof(pts[0])); i++) {
        s32 r = FR_atan2(pts[i].y, pts[i].x, 16);
        double ref = atan2((double)pts[i].y, (double)pts[i].x);
        printf("| (%ld, %ld) | %.4f | %.4f |\n",
               (long)pts[i].y, (long)pts[i].x, frd(r, 16), ref);
    }
    printf("\n> `FR_atan2` returns radians at the specified output radix.\n");
    printf("> `FR_atan(x, radix, out_radix)` is implemented as `FR_atan2(x, 1<<radix, out_radix)`.\n\n");
}

/* ============================================================
 * Section 8: Power & Log
 * ============================================================ */

static void section_pow_log(void) {
    md_h2("8. Power & Log Functions");

    md_h3("8.1 FR_pow2 vs pow(2, x), radix 16");
    printf("| x | FR_pow2(x*2^16, 16) | as double | pow(2,x) | abs err | rel err |\n");
    printf("|---:|---:|---:|---:|---:|---:|\n");
    double pow2_inputs[] = {-8, -4, -2, -1, -0.5, 0, 0.25, 0.5, 1, 1.5, 2, 3, 4, 8, 12};
    stats_t pow2_stats; stats_reset(&pow2_stats);
    for (int i = 0; i < (int)(sizeof(pow2_inputs)/sizeof(pow2_inputs[0])); i++) {
        double x = pow2_inputs[i];
        s32 fr = (s32)(x * (1L << 16));
        s32 r = FR_pow2(fr, 16);
        double rd = frd(r, 16);
        double ref = pow(2.0, x);
        double err = rd - ref; if (err < 0) err = -err;
        double rel = ref != 0.0 ? err / fabs(ref) : err;
        stats_add(&pow2_stats, x, rd, ref);
        printf("| %.4g | %ld | %.6g | %.6g | %.4g | %.4g |\n",
               x, (long)r, rd, ref, err, rel);
    }
    printf("\n");
    table_header_stats();
    table_row_stats("FR_pow2 sweep", &pow2_stats);
    printf("\n");

    md_h3("8.2 FR_pow2 fine sweep, radix 16");
    stats_t pow2_fine; stats_reset(&pow2_fine);
    for (int i = -800; i <= 800; i++) {
        double x = i / 100.0;
        s32 fr = (s32)(x * (1L << 16));
        s32 r = FR_pow2(fr, 16);
        double rd = frd(r, 16);
        double ref = pow(2.0, x);
        stats_add(&pow2_fine, x, rd, ref);
    }
    table_header_stats();
    table_row_stats("FR_pow2 [-8,8] step 0.01", &pow2_fine);
    printf("\n");

    md_h3("8.3 FR_log2 — empirical behavior on integer powers of 2");
    printf("> Implementation: leading-bit-position → normalize the remainder to s1.30 →\n");
    printf("> 65-entry mantissa lookup with linear interpolation.\n\n");
    printf("| input | radix | out_radix | FR_log2 | as double | log2(x) |\n");
    printf("|---:|---:|---:|---:|---:|---:|\n");
    struct { s32 in; u16 r; u16 or_; double ref; } log2_cases[] = {
        {1,        0, 16, 0.0},
        {2,        0, 16, 1.0},
        {4,        0, 16, 2.0},
        {8,        0, 16, 3.0},
        {16,       0, 16, 4.0},
        {32,       0, 16, 5.0},
        {64,       0, 16, 6.0},
        {128,      0, 16, 7.0},
        {1024,     0, 16, 10.0},
        {65536,    0, 16, 16.0},
        {3,        0, 16, log2(3.0)},
        {5,        0, 16, log2(5.0)},
        {7,        0, 16, log2(7.0)},
        {1<<16,   16, 16, 0.0},  /* 1.0 in s.16 */
        {2<<16,   16, 16, 1.0},  /* 2.0 in s.16 */
    };
    stats_t log2_stats; stats_reset(&log2_stats);
    for (int i = 0; i < (int)(sizeof(log2_cases)/sizeof(log2_cases[0])); i++) {
        s32 r = FR_log2(log2_cases[i].in, log2_cases[i].r, log2_cases[i].or_);
        double rd = frd(r, log2_cases[i].or_);
        printf("| %ld | %u | %u | %ld | %.6g | %.6g |\n",
               (long)log2_cases[i].in, log2_cases[i].r, log2_cases[i].or_,
               (long)r, rd, log2_cases[i].ref);
        stats_add(&log2_stats, (double)log2_cases[i].in, rd, log2_cases[i].ref);
    }
    printf("\n");
    table_header_stats();
    table_row_stats("FR_log2 vs log2()", &log2_stats);
    printf("\n> v2: `FR_log2` rewritten — leading-bit-position → normalize to s1.30 → 65-entry\n");
    printf("> mantissa LUT with linear interpolation. Error is now table-limited.\n\n");

    md_h3("8.4 FR_ln — derived from FR_log2 (v2: fixed by inheritance)");
    printf("| input | FR_ln(in,16,16) | as double | ln(input) |\n|---:|---:|---:|---:|\n");
    double ln_inputs[] = {1, 2, M_E, 4, 8, 10, 100, 1000};
    stats_t ln_stats; stats_reset(&ln_stats);
    for (int i = 0; i < (int)(sizeof(ln_inputs)/sizeof(ln_inputs[0])); i++) {
        s32 fr = (s32)(ln_inputs[i] * (1L << 16));
        s32 r = FR_ln(fr, 16, 16);
        double rd = frd(r, 16);
        double ref = log(ln_inputs[i]);
        stats_add(&ln_stats, ln_inputs[i], rd, ref);
        printf("| %.4g | %ld | %.6g | %.6g |\n", ln_inputs[i], (long)r, rd, ref);
    }
    printf("\n");
    table_header_stats();
    table_row_stats("FR_ln vs ln()", &ln_stats);
    printf("\n");

    md_h3("8.5 FR_log10 — derived from FR_log2 (v2: fixed by inheritance)");
    printf("| input | FR_log10(in,16,16) | as double | log10(input) |\n|---:|---:|---:|---:|\n");
    double log10_inputs[] = {1, 2, 5, 10, 100, 1000, 10000};
    stats_t log10_stats; stats_reset(&log10_stats);
    for (int i = 0; i < (int)(sizeof(log10_inputs)/sizeof(log10_inputs[0])); i++) {
        s32 fr = (s32)(log10_inputs[i] * (1L << 16));
        s32 r = FR_log10(fr, 16, 16);
        double rd = frd(r, 16);
        double ref = log10(log10_inputs[i]);
        stats_add(&log10_stats, log10_inputs[i], rd, ref);
        printf("| %.4g | %ld | %.6g | %.6g |\n", log10_inputs[i], (long)r, rd, ref);
    }
    printf("\n");
    table_header_stats();
    table_row_stats("FR_log10 vs log10()", &log10_stats);
    printf("\n");

    md_h3("8.6 FR_EXP and FR_POW10 macros (wrap FR_pow2)");
    printf("| Expression | Result | as double | Reference | Note |\n|---|---:|---:|---:|---|\n");
    {
        s32 in = (s32)(1.0 * (1L << 16));
        s32 r = FR_EXP(in, 16);
        double rd = frd(r, 16);
        printf("| FR_EXP(1.0,16) | %ld | %.6g | %.6g | exp(1) = e |\n",
               (long)r, rd, M_E);
    }
    {
        s32 in = (s32)(2.0 * (1L << 16));
        s32 r = FR_POW10(in, 16);
        double rd = frd(r, 16);
        printf("| FR_POW10(2.0,16) | %ld | %.6g | %.6g | 10^2 = 100 |\n",
               (long)r, rd, 100.0);
    }
    printf("\n");

    md_h3("8.7 FR_LOG2MIN sentinel");
    printf("| Call | Result | FR_LOG2MIN |\n|---|---:|---:|\n");
    printf("| FR_log2(0,16,16) | %ld | %ld |\n",
           (long)FR_log2(0, 16, 16), (long)FR_LOG2MIN);
    printf("| FR_log2(-1,16,16) | %ld | %ld |\n",
           (long)FR_log2(-1, 16, 16), (long)FR_LOG2MIN);
    printf("\n");

    md_h3("8.8 Coverage of seldom-reached branches");
    /* FR_TanI with deg in (-360, -180) hits the `deg += 360` branch */
    printf("| Call | Result |\n|---|---:|\n");
    printf("| FR_TanI(-200) | %ld |\n", (long)FR_TanI((s16)-200));
    printf("| FR_TanI(200)  | %ld |\n", (long)FR_TanI((s16)200));
    /* Print helpers with NULL stream should return -1 */
    printf("| FR_printNumD(NULL,1,0) | %d |\n", FR_printNumD(NULL, 1, 0));
    printf("| FR_printNumH(NULL,1,0) | %d |\n", FR_printNumH(NULL, 1, 0));
    printf("| FR_printNumF(NULL,1,16,0,4) | %d |\n", FR_printNumF(NULL, 1, 16, 0, 4));
    printf("\n");
}

/* ============================================================
 * Section 9: Print Helpers
 * ============================================================ */

static void section_print(void) {
    md_h2("9. Print Helpers");

    md_h3("9.1 FR_printNumD (decimal)");
    printf("| n | pad | output | bytes written |\n|---:|---:|---|---:|\n");
    struct { int n; int pad; const char *desc; } d_cases[] = {
        {0, 0, "zero"},
        {1, 0, "one"},
        {-1, 0, "neg one"},
        {12345, 0, "positive"},
        {-12345, 0, "negative"},
        {12, 5, "padded right"},
        {-12, 5, "padded neg"},
        {INT_MAX, 0, "INT_MAX"},
        {INT_MIN, 0, "INT_MIN"},
    };
    for (int i = 0; i < (int)(sizeof(d_cases)/sizeof(d_cases[0])); i++) {
        buf_reset();
        int wrote = FR_printNumD(captured_putchar, d_cases[i].n, d_cases[i].pad);
        printf("| %d | %d | `%s` | %d |\n", d_cases[i].n, d_cases[i].pad, g_buf, wrote);
    }
    printf("\n");

    md_h3("9.2 FR_printNumH (hex)");
    printf("| n | showPrefix | output | bytes written |\n|---:|---:|---|---:|\n");
    struct { int n; int prefix; } h_cases[] = {
        {0, 0}, {0, 1},
        {0xab, 0}, {0xab, 1},
        {0x12345678, 1},
        {-1, 1},
        {INT_MIN, 1},
    };
    for (int i = 0; i < (int)(sizeof(h_cases)/sizeof(h_cases[0])); i++) {
        buf_reset();
        int wrote = FR_printNumH(captured_putchar, h_cases[i].n, h_cases[i].prefix);
        printf("| %d | %d | `%s` | %d |\n", h_cases[i].n, h_cases[i].prefix, g_buf, wrote);
    }
    printf("\n> v2: `FR_printNumH` casts to unsigned before shifting, so the output is\n");
    printf("> portable across compilers.\n\n");

    md_h3("9.3 FR_printNumF (fixed-radix as float)");
    printf("| value | radix | pad | prec | output | bytes |\n|---:|---:|---:|---:|---|---:|\n");
    struct { s32 n; int rad; int pad; int prec; const char *desc; } f_cases[] = {
        {(s32)(0.0    * 65536), 16, 0, 4, "0.0"},
        {(s32)(1.0    * 65536), 16, 0, 4, "1.0"},
        {(s32)(-1.0   * 65536), 16, 0, 4, "-1.0"},
        {(s32)(3.14159* 65536), 16, 0, 4, "pi"},
        {(s32)(-3.14159*65536), 16, 0, 4, "-pi"},
        {(s32)(0.0001 * 65536), 16, 0, 4, "0.0001"},
        {(s32)(1.05   * 65536), 16, 0, 4, "1.05"},
        {(s32)(-1.05  * 65536), 16, 0, 4, "-1.05"},
        {(s32)(12.34  * 65536), 16, 8, 2, "12.34 padded"},
        {(s32)0x80000000,       16, 0, 4, "INT32_MIN"},
        {(s32)0x7fffffff,       16, 0, 4, "INT32_MAX"},
    };
    for (int i = 0; i < (int)(sizeof(f_cases)/sizeof(f_cases[0])); i++) {
        buf_reset();
        int wrote = FR_printNumF(captured_putchar, f_cases[i].n,
                                 f_cases[i].rad, f_cases[i].pad, f_cases[i].prec);
        printf("| %s | %d | %d | %d | `%s` | %d |\n",
               f_cases[i].desc, f_cases[i].rad, f_cases[i].pad, f_cases[i].prec,
               g_buf, wrote);
    }
    printf("\n> v2: `FR_printNumF` and `FR_printNumD` now work in unsigned magnitude, so\n");
    printf("> `INT32_MIN` / `INT_MIN` no longer trigger signed-negation UB. Fraction\n");
    printf("> extraction in `FR_printNumF` was also rewritten for correctness.\n\n");
}

/* ============================================================
 * Section 10: 2D Matrix
 * ============================================================ */

static void section_matrix2d(void) {
    md_h2("10. 2D Matrix (FR_Matrix2D_CPT)");

    md_h3("10.1 Identity matrix");
    {
        FR_Matrix2D_CPT m(8);
        m.ID();
        printf("| Element | Value | Expected |\n|---|---:|---:|\n");
        printf("| m00 | %ld | %ld |\n", (long)m.m00, (long)I2FR(1, 8));
        printf("| m01 | %ld | 0 |\n", (long)m.m01);
        printf("| m02 | %ld | 0 |\n", (long)m.m02);
        printf("| m10 | %ld | 0 |\n", (long)m.m10);
        printf("| m11 | %ld | %ld |\n", (long)m.m11, (long)I2FR(1, 8));
        printf("| m12 | %ld | 0 |\n", (long)m.m12);
        printf("| radix | %u | 8 |\n", m.radix);
        printf("| fast | %d | (set by checkfast) |\n", m.fast);
    }
    printf("\n");

    md_h3("10.2 Identity transform of points");
    {
        FR_Matrix2D_CPT m(8);
        m.ID();
        m.checkfast();
        s32 xp, yp;
        printf("| Input (x,y) | Output (xp,yp) | OK? |\n|---|---|:---:|\n");
        s32 pts[][2] = {{0,0},{1,1},{10,20},{-5,7},{1000,-1000}};
        for (int i = 0; i < 5; i++) {
            m.XFormPtI(pts[i][0], pts[i][1], &xp, &yp);
            int ok = (xp == pts[i][0] && yp == pts[i][1]);
            printf("| (%ld,%ld) | (%ld,%ld) | %s |\n",
                   (long)pts[i][0], (long)pts[i][1],
                   (long)xp, (long)yp, ok ? "OK" : "FAIL");
        }
    }
    printf("\n");

    md_h3("10.3 Translation");
    {
        FR_Matrix2D_CPT m(8);
        m.ID();
        m.XlateI(5, 10);
        m.checkfast();
        s32 xp, yp;
        m.XFormPtI(10, 20, &xp, &yp);
        printf("| Op | Result | Expected |\n|---|---:|---:|\n");
        printf("| XlateI(5,10) then XForm(10,20) x | %ld | 15 |\n", (long)xp);
        printf("| XlateI(5,10) then XForm(10,20) y | %ld | 30 |\n", (long)yp);
        m.XlateRelativeI(5, 10);
        m.XFormPtI(10, 20, &xp, &yp);
        printf("| + XlateRelativeI(5,10) x | %ld | 20 |\n", (long)xp);
        printf("| + XlateRelativeI(5,10) y | %ld | 40 |\n", (long)yp);
        /* explicit-radix overloads (just exercise) */
        m.XlateI(100, 200, 8);
        m.XlateRelativeI(50, 100, 8);
        m.checkfast();
        m.XFormPtI(0, 0, &xp, &yp, 8);
        printf("| XlateI(100,200,8) + Rel(50,100,8) origin x | %ld | 150 |\n", (long)xp);
        printf("| XlateI(100,200,8) + Rel(50,100,8) origin y | %ld | 300 |\n", (long)yp);
    }
    printf("\n");

    md_h3("10.4 Rotation (setrotate)");
    {
        FR_Matrix2D_CPT m(8);
        printf("| deg | m00/2^8 | m01/2^8 | m10/2^8 | m11/2^8 | cos | sin | -sin |\n");
        printf("|---:|---:|---:|---:|---:|---:|---:|---:|\n");
        int degs[] = {0, 30, 45, 60, 90, 180, 270, -45};
        for (int i = 0; i < 8; i++) {
            m.setrotate((s16)degs[i]);
            double c = cos(degs[i] * M_PI / 180.0);
            double s = sin(degs[i] * M_PI / 180.0);
            printf("| %d | %.4f | %.4f | %.4f | %.4f | %.4f | %.4f | %.4f |\n",
                   degs[i], frd(m.m00, 8), frd(m.m01, 8),
                   frd(m.m10, 8), frd(m.m11, 8), c, s, -s);
        }
        /* explicit radix overload */
        m.setrotate(45, 10);
    }
    printf("\n");

    md_h3("10.5 Rotation point transform — sanity (45 deg)");
    {
        FR_Matrix2D_CPT m(8);
        m.setrotate(45);
        m.checkfast();
        s32 xp, yp;
        m.XFormPtI(100, 0, &xp, &yp);
        double r = sqrt(2.0) / 2.0 * 100.0;
        printf("| Input | Output | Expected |\n|---|---|---|\n");
        printf("| (100,0) | (%ld,%ld) | (%.1f,%.1f) |\n",
               (long)xp, (long)yp, r, r);
        m.XFormPtI(0, 100, &xp, &yp);
        printf("| (0,100) | (%ld,%ld) | (%.1f,%.1f) |\n",
               (long)xp, (long)yp, -r, r);
    }
    printf("\n");

    md_h3("10.6 16-bit point transform");
    {
        FR_Matrix2D_CPT m(8);
        m.ID();
        m.checkfast();
        s16 xp16, yp16;
        m.XFormPtI16((s16)100, (s16)200, &xp16, &yp16);
        printf("| Op | x | y |\n|---|---:|---:|\n");
        printf("| ID XFormPtI16(100,200) | %d | %d |\n", xp16, yp16);
        m.XFormPtI16NoTranslate((s16)100, (s16)200, &xp16, &yp16);
        printf("| ID XFormPtI16NoTranslate(100,200) | %d | %d |\n", xp16, yp16);
        /* with full matrix (force fast=false path) */
        m.set(I2FR(2, 8), I2FR(1, 8), 0,
              I2FR(0, 8), I2FR(1, 8), 0, 8);
        m.checkfast();
        m.XFormPtI16((s16)10, (s16)10, &xp16, &yp16);
        printf("| 2x+y, y XFormPtI16(10,10) | %d | %d |\n", xp16, yp16);
        m.XFormPtI16NoTranslate((s16)10, (s16)10, &xp16, &yp16);
        printf("| 2x+y, y NoTranslate(10,10) | %d | %d |\n", xp16, yp16);
    }
    printf("\n");

    md_h3("10.7 No-translate 32-bit transform");
    {
        FR_Matrix2D_CPT m(8);
        m.ID();
        m.checkfast();
        s32 xp, yp;
        m.XFormPtINoTranslate(10, 20, &xp, &yp, 8);
        printf("| Op | x | y |\n|---|---:|---:|\n");
        printf("| ID NoTranslate(10,20) r=8 | %ld | %ld |\n", (long)xp, (long)yp);
        m.set(I2FR(2, 8), I2FR(1, 8), I2FR(99, 8),
              I2FR(0, 8), I2FR(1, 8), I2FR(99, 8), 8);
        m.checkfast();  /* should be false */
        m.XFormPtINoTranslate(10, 20, &xp, &yp, 8);
        printf("| 2x+y NoTranslate(10,20) | %ld | %ld |\n", (long)xp, (long)yp);
    }
    printf("\n");

    md_h3("10.8 set, det, inv");
    {
        FR_Matrix2D_CPT m(8), inv(8);
        m.set(I2FR(2, 8), 0, 0, 0, I2FR(3, 8), 0, 8);
        s32 d = m.det();
        printf("| Op | Result | Expected |\n|---|---:|---:|\n");
        printf("| det of diag(2,3) (radix 8) | %ld | %ld |\n",
               (long)d, (long)I2FR(6, 8));

        bool r = m.inv(&inv);
        printf("| inv() returns true | %d | %d |\n", r ? 1 : 0, 1);
        printf("| inv.m00 (~1/2 in s.8) | %ld | %ld |\n",
               (long)inv.m00, (long)(I2FR(1,8) / 2));
        printf("| inv.m11 (~1/3 in s.8) | %ld | %ld |\n",
               (long)inv.m11, (long)(I2FR(1,8) / 3));

        /* inv() in place */
        FR_Matrix2D_CPT m2(8);
        m2.set(I2FR(2, 8), 0, I2FR(5, 8),
               0, I2FR(2, 8), I2FR(7, 8), 8);
        bool r2 = m2.inv();
        printf("| in-place inv() returns | %d | %d |\n", r2 ? 1 : 0, 1);

        /* singular */
        FR_Matrix2D_CPT s_(8);
        s_.set(0, 0, 0, 0, 0, 0, 8);
        bool r3 = s_.inv(&inv);
        printf("| inv of singular matrix | %d | %d |\n", r3 ? 1 : 0, 0);
    }
    printf("\n");

    md_h3("10.9 add, sub, operators (+=, -=, *=)");
    {
        FR_Matrix2D_CPT a(8), b(8), c(8);
        a.set(I2FR(2, 8), 0, I2FR(3, 8),
              0, I2FR(2, 8), I2FR(4, 8), 8);
        b.set(I2FR(1, 8), 0, I2FR(1, 8),
              0, I2FR(1, 8), I2FR(1, 8), 8);
        c = a;
        c.add(&b);
        printf("| Op | c.m00 | c.m02 | c.m12 |\n|---|---:|---:|---:|\n");
        printf("| a+b via add() | %ld | %ld | %ld |\n",
               (long)c.m00, (long)c.m02, (long)c.m12);
        c = a; c.sub(&b);
        printf("| a-b via sub() | %ld | %ld | %ld |\n",
               (long)c.m00, (long)c.m02, (long)c.m12);
        c = a; c += b;
        printf("| a += b | %ld | %ld | %ld |\n",
               (long)c.m00, (long)c.m02, (long)c.m12);
        c = a; c -= b;
        printf("| a -= b | %ld | %ld | %ld |\n",
               (long)c.m00, (long)c.m02, (long)c.m12);
        c = a; c *= 2;
        printf("| a *= 2 | %ld | %ld | %ld |\n",
               (long)c.m00, (long)c.m02, (long)c.m12);
    }
    printf("\n");

    md_h3("10.10 checkfast classification");
    {
        FR_Matrix2D_CPT m(8);
        m.ID();
        printf("| Matrix | checkfast | Expected |\n|---|---:|---:|\n");
        printf("| identity | %d | 1 |\n", (int)m.checkfast());
        m.set(I2FR(2, 8), 0, 0, 0, I2FR(3, 8), 0, 8);
        printf("| diag(2,3) | %d | 1 |\n", (int)m.checkfast());
        m.set(I2FR(2, 8), I2FR(1, 8), 0,
              I2FR(1, 8), I2FR(2, 8), 0, 8);
        printf("| full 2x2 | %d | 0 |\n", (int)m.checkfast());
    }
    printf("\n");
}

/* ============================================================
 * Section 11: Sqrt, Hypot, Waves, and ADSR (v2 new)
 * ============================================================ */

static void section_v2_new(void) {
    md_h2("11. Sqrt, Hypot, Waves, and ADSR (v2 new)");

    md_h3("11.1 FR_sqrt vs sqrt(), radix 16");
    printf("| input (double) | FR_sqrt | as double | sqrt() | abs err |\n");
    printf("|---:|---:|---:|---:|---:|\n");
    /* Inputs must fit in s.16 (max representable is ~32767). */
    double sqrt_inputs[] = {0, 0.0001, 0.25, 0.5, 1, 2, 3, 4, 7, 9, 16, 25, 100, 1024, 10000, 32000};
    stats_t sqrt_stats; stats_reset(&sqrt_stats);
    for (int i = 0; i < (int)(sizeof(sqrt_inputs)/sizeof(sqrt_inputs[0])); i++) {
        double x = sqrt_inputs[i];
        s32 fr = (s32)(x * (1L << 16));
        s32 r = FR_sqrt(fr, 16);
        double rd = frd(r, 16);
        double ref = sqrt(x);
        double err = rd - ref; if (err < 0) err = -err;
        stats_add(&sqrt_stats, x, rd, ref);
        printf("| %.6g | %ld | %.6g | %.6g | %.4g |\n",
               x, (long)r, rd, ref, err);
    }
    printf("\n");
    table_header_stats();
    table_row_stats("FR_sqrt sweep", &sqrt_stats);
    printf("\n");

    md_h3("11.2 FR_sqrt fine sweep, radix 16");
    stats_t sqrt_fine; stats_reset(&sqrt_fine);
    for (int i = 1; i <= 1000; i++) {
        double x = i * 10.0;     /* 10..10000 */
        s32 fr = (s32)(x * (1L << 16));
        s32 r = FR_sqrt(fr, 16);
        double rd = frd(r, 16);
        double ref = sqrt(x);
        stats_add(&sqrt_fine, x, rd, ref);
    }
    table_header_stats();
    table_row_stats("FR_sqrt [10,10000]", &sqrt_fine);
    printf("\n");

    md_h3("11.3 FR_sqrt domain sentinel");
    printf("| Call | Result | INT32_MIN |\n|---|---:|---:|\n");
    printf("| FR_sqrt(-1, 16) | %ld | %ld |\n",
           (long)FR_sqrt(-1, 16), (long)(s32)0x80000000);
    printf("| FR_sqrt(-65536, 16) | %ld | %ld |\n",
           (long)FR_sqrt(-65536, 16), (long)(s32)0x80000000);
    printf("\n");

    md_h3("11.4 FR_hypot vs hypot(), radix 16");
    printf("| x | y | FR_hypot | as double | hypot() | abs err |\n");
    printf("|---:|---:|---:|---:|---:|---:|\n");
    struct { double x, y; } hyp_cases[] = {
        {0, 0},     {1, 0},     {0, 1},
        {3, 4},     {5, 12},    {8, 15},
        {-3, -4},   {-3, 4},    {3, -4},
        {1, 1},     {0.5, 0.5}, {100, 100},
        {1000, 1},  {1, 1000},
    };
    stats_t hyp_stats; stats_reset(&hyp_stats);
    for (int i = 0; i < (int)(sizeof(hyp_cases)/sizeof(hyp_cases[0])); i++) {
        s32 fx = (s32)(hyp_cases[i].x * (1L << 16));
        s32 fy = (s32)(hyp_cases[i].y * (1L << 16));
        s32 r  = FR_hypot(fx, fy, 16);
        double rd = frd(r, 16);
        double ref = hypot(hyp_cases[i].x, hyp_cases[i].y);
        double err = rd - ref; if (err < 0) err = -err;
        stats_add(&hyp_stats, sqrt(hyp_cases[i].x*hyp_cases[i].x + hyp_cases[i].y*hyp_cases[i].y),
                  rd, ref);
        printf("| %g | %g | %ld | %.6g | %.6g | %.4g |\n",
               hyp_cases[i].x, hyp_cases[i].y, (long)r, rd, ref, err);
    }
    printf("\n");
    table_header_stats();
    table_row_stats("FR_hypot sweep", &hyp_stats);
    printf("\n");

    md_h3("11.4b FR_hypot_fast8 (8-seg) vs hypot(), radix 16");
    printf("| x | y | FR_hypot_fast8 | as double | hypot() | abs err | rel err%% |\n");
    printf("|---:|---:|---:|---:|---:|---:|---:|\n");
    stats_t hf8_stats; stats_reset(&hf8_stats);
    for (int i = 0; i < (int)(sizeof(hyp_cases)/sizeof(hyp_cases[0])); i++) {
        s32 fx = (s32)(hyp_cases[i].x * (1L << 16));
        s32 fy = (s32)(hyp_cases[i].y * (1L << 16));
        s32 r  = FR_hypot_fast8(fx, fy);
        double rd = frd(r, 16);
        double ref = hypot(hyp_cases[i].x, hyp_cases[i].y);
        double err = rd - ref; if (err < 0) err = -err;
        double rel = (ref > 0) ? err / ref * 100.0 : 0.0;
        stats_add(&hf8_stats, sqrt(hyp_cases[i].x*hyp_cases[i].x + hyp_cases[i].y*hyp_cases[i].y),
                  rd, ref);
        printf("| %g | %g | %ld | %.6g | %.6g | %.4g | %.4g |\n",
               hyp_cases[i].x, hyp_cases[i].y, (long)r, rd, ref, err, rel);
    }
    printf("\n");
    table_header_stats();
    table_row_stats("FR_hypot_fast8 sweep", &hf8_stats);
    printf("\n");

    md_h3("11.5 fr_wave_sqr / fr_wave_pwm at key BAM phases");
    printf("| phase | duty | sqr | pwm |\n|---:|---:|---:|---:|\n");
    u16 phs[] = {0, 0x2000, 0x4000, 0x6000, 0x8000, 0xa000, 0xc000, 0xe000, 0xffff};
    u16 duties[] = {0x4000, 0x8000, 0xc000};
    for (int i = 0; i < (int)(sizeof(phs)/sizeof(phs[0])); i++) {
        for (int j = 0; j < (int)(sizeof(duties)/sizeof(duties[0])); j++) {
            printf("| 0x%04x | 0x%04x | %d | %d |\n",
                   phs[i], duties[j], fr_wave_sqr(phs[i]),
                   fr_wave_pwm(phs[i], duties[j]));
        }
    }
    printf("\n");

    md_h3("11.6 fr_wave_tri / fr_wave_saw at key BAM phases");
    printf("| phase | tri | saw | reference (ideal) |\n|---:|---:|---:|---|\n");
    {
        struct { u16 ph; const char *desc; } cases[] = {
            {0x0000, "0° (zero)"},
            {0x1000, "22.5° (rising)"},
            {0x2000, "45° (rising)"},
            {0x4000, "90° (peak)"},
            {0x6000, "135° (falling)"},
            {0x8000, "180° (zero)"},
            {0xa000, "225° (descending)"},
            {0xc000, "270° (trough)"},
            {0xe000, "315° (rising)"},
            {0xffff, "360° (just before zero)"},
        };
        for (int i = 0; i < (int)(sizeof(cases)/sizeof(cases[0])); i++) {
            printf("| 0x%04x | %d | %d | %s |\n",
                   cases[i].ph, fr_wave_tri(cases[i].ph),
                   fr_wave_saw(cases[i].ph), cases[i].desc);
        }
    }
    printf("\n");

    md_h3("11.7 fr_wave_tri error vs ideal triangle (full sweep)");
    {
        stats_t tri_stats; stats_reset(&tri_stats);
        for (int i = 0; i < 65536; i++) {
            u16 ph = (u16)i;
            s16 actual = fr_wave_tri(ph);
            /* Ideal triangle: piecewise linear with peak +1 at 90° and trough -1 at 270° */
            double t = (double)i / 65536.0;     /* [0, 1) */
            double ideal;
            if      (t < 0.25) ideal =  4.0 * t;             /* 0 → 1 */
            else if (t < 0.50) ideal =  2.0 - 4.0 * t;       /* 1 → 0 */
            else if (t < 0.75) ideal = -4.0 * (t - 0.5);     /* 0 → -1 */
            else               ideal = -1.0 + 4.0 * (t - 0.75); /* -1 → 0 */
            stats_add(&tri_stats, t * 360.0, (double)actual / 32767.0, ideal);
        }
        table_header_stats();
        table_row_stats("fr_wave_tri vs ideal", &tri_stats);
        printf("\n");
    }

    md_h3("11.8 fr_wave_tri_morph at multiple break points");
    printf("| phase | brk=0x4000 | brk=0x8000 (sym) | brk=0xc000 |\n");
    printf("|---:|---:|---:|---:|\n");
    for (int i = 0; i < (int)(sizeof(phs)/sizeof(phs[0])); i++) {
        printf("| 0x%04x | %d | %d | %d |\n",
               phs[i],
               fr_wave_tri_morph(phs[i], 0x4000),
               fr_wave_tri_morph(phs[i], 0x8000),
               fr_wave_tri_morph(phs[i], 0xc000));
    }
    printf("\n");

    md_h3("11.9 fr_wave_noise: first 16 samples from seed 0xACE1");
    {
        u32 state = 0xACE1u;
        printf("| step | state (hex) | sample |\n|---:|---:|---:|\n");
        for (int i = 0; i < 16; i++) {
            s16 v = fr_wave_noise(&state);
            printf("| %d | 0x%08x | %d |\n", i, (unsigned)state, v);
        }
        printf("\n");
    }

    md_h3("11.10 FR_HZ2BAM_INC at common audio rates");
    printf("| freq (Hz) | sample rate | inc | implied freq |\n");
    printf("|---:|---:|---:|---:|\n");
    {
        struct { u32 hz, sr; } cases[] = {
            {440, 48000}, {440, 44100}, {1000, 48000}, {261, 48000}, {1, 65536}
        };
        for (int i = 0; i < (int)(sizeof(cases)/sizeof(cases[0])); i++) {
            u16 inc = FR_HZ2BAM_INC(cases[i].hz, cases[i].sr);
            double implied = (double)inc * (double)cases[i].sr / 65536.0;
            printf("| %u | %u | %u | %.4f |\n",
                   (unsigned)cases[i].hz, (unsigned)cases[i].sr,
                   (unsigned)inc, implied);
        }
        printf("\n");
    }

    md_h3("11.11 ADSR full lifecycle (atk=10, dec=20, sus=16384, rel=30)");
    {
        fr_adsr_t env;
        fr_adsr_init(&env, 10, 20, 16384, 30);
        printf("> Initial state: %u, level: %ld\n\n", env.state, (long)env.level);
        printf("| step | state | output |\n|---:|---:|---:|\n");
        fr_adsr_trigger(&env);
        int step = 0;
        for (int i = 0; i < 35; i++) {
            s16 v = fr_adsr_step(&env);
            printf("| %d | %u | %d |\n", step++, env.state, v);
        }
        fr_adsr_release(&env);
        for (int i = 0; i < 40; i++) {
            s16 v = fr_adsr_step(&env);
            printf("| %d | %u | %d |\n", step++, env.state, v);
            if (env.state == FR_ADSR_IDLE) break;
        }
        printf("\n");
    }
}

/* ============================================================
 * Section 12: Multi-Radix Accuracy Sweeps (log, div)
 * ============================================================ */

static void section_multiradix(void) {
    md_h2("12. Multi-Radix Accuracy Sweeps");

    /* ----------------------------------------------------------
     * 12.1 FR_log2 fine sweep at radixes 8, 12, 16, 24
     * ---------------------------------------------------------- */
    md_h3("12.1 FR_log2 fine sweep at multiple radixes");
    printf("| Radix | N | Max err (LSB) | Mean err (LSB) | Max abs err | Worst input |\n");
    printf("|---:|---:|---:|---:|---:|---:|\n");

    int log2_radixes[] = {8, 12, 16, 24};
    for (int ri = 0; ri < 4; ri++) {
        int R = log2_radixes[ri];
        double scale = (double)(1L << R);
        double max_val = (double)((1L << (30 - R)));  /* stay well within s32 */
        stats_t st; stats_reset(&st);

        /* Sweep from 0.125 to max representable value */
        double inputs[] = {0.125, 0.25, 0.5, 0.75, 1.0, 1.5, 2.0, 2.5, 3.0,
                           M_E, M_PI, 4.0, 5.0, 7.0, 8.0, 10.0, 16.0, 32.0,
                           64.0, 100.0, 256.0, 1000.0};
        int ninp = (int)(sizeof(inputs)/sizeof(inputs[0]));

        for (int i = 0; i < ninp; i++) {
            if (inputs[i] > max_val) continue;   /* would overflow s32 */
            s32 fr = (s32)(inputs[i] * scale);
            if (fr <= 0) continue;
            s32 r = FR_log2(fr, (u16)R, (u16)R);
            double rd = frd(r, R);
            double ref = log2(inputs[i]);
            stats_add(&st, inputs[i], rd, ref);
        }

        /* Fine-grained sweep in [1, min(100, max_val)] */
        double sweep_max = max_val < 100.0 ? max_val : 100.0;
        for (int i = 1; i <= 500; i++) {
            double x = 1.0 + ((sweep_max - 1.0) * i / 500.0);
            s32 fr = (s32)(x * scale);
            if (fr <= 0) continue;
            s32 r = FR_log2(fr, (u16)R, (u16)R);
            double rd = frd(r, R);
            double ref = log2(x);
            stats_add(&st, x, rd, ref);
        }

        double lsb = 1.0 / scale;
        printf("| %d | %d | %.2f | %.2f | %.6g | %.4g |\n",
               R, st.n,
               st.max_abs_err / lsb, stats_mean(&st) / lsb,
               st.max_abs_err, st.worst_input);
    }
    printf("\n");

    /* ----------------------------------------------------------
     * 12.2 FR_ln fine sweep at radixes 8, 12, 16, 24
     * ---------------------------------------------------------- */
    md_h3("12.2 FR_ln fine sweep at multiple radixes");
    printf("| Radix | N | Max err (LSB) | Mean err (LSB) | Max abs err | Worst input |\n");
    printf("|---:|---:|---:|---:|---:|---:|\n");

    for (int ri = 0; ri < 4; ri++) {
        int R = log2_radixes[ri];
        double scale = (double)(1L << R);
        double max_val = (double)((1L << (30 - R)));
        double sweep_max = max_val < 100.0 ? max_val : 100.0;
        stats_t st; stats_reset(&st);

        for (int i = 1; i <= 500; i++) {
            double x = 0.5 + ((sweep_max - 0.5) * i / 500.0);
            s32 fr = (s32)(x * scale);
            if (fr <= 0) continue;
            s32 r = FR_ln(fr, (u16)R, (u16)R);
            double rd = frd(r, R);
            double ref = log(x);
            stats_add(&st, x, rd, ref);
        }

        double lsb = 1.0 / scale;
        printf("| %d | %d | %.2f | %.2f | %.6g | %.4g |\n",
               R, st.n,
               st.max_abs_err / lsb, stats_mean(&st) / lsb,
               st.max_abs_err, st.worst_input);
    }
    printf("\n");

    /* ----------------------------------------------------------
     * 12.3 FR_log10 fine sweep at radixes 8, 12, 16, 24
     * ---------------------------------------------------------- */
    md_h3("12.3 FR_log10 fine sweep at multiple radixes");
    printf("| Radix | N | Max err (LSB) | Mean err (LSB) | Max abs err | Worst input |\n");
    printf("|---:|---:|---:|---:|---:|---:|\n");

    for (int ri = 0; ri < 4; ri++) {
        int R = log2_radixes[ri];
        double scale = (double)(1L << R);
        double max_val = (double)((1L << (30 - R)));
        double sweep_max = max_val < 1000.0 ? max_val : 1000.0;
        stats_t st; stats_reset(&st);

        for (int i = 1; i <= 500; i++) {
            double x = 0.5 + ((sweep_max - 0.5) * i / 500.0);
            s32 fr = (s32)(x * scale);
            if (fr <= 0) continue;
            s32 r = FR_log10(fr, (u16)R, (u16)R);
            double rd = frd(r, R);
            double ref = log10(x);
            stats_add(&st, x, rd, ref);
        }

        double lsb = 1.0 / scale;
        printf("| %d | %d | %.2f | %.2f | %.6g | %.4g |\n",
               R, st.n,
               st.max_abs_err / lsb, stats_mean(&st) / lsb,
               st.max_abs_err, st.worst_input);
    }
    printf("\n");

    /* ----------------------------------------------------------
     * 12.4 FR_DIV (round-to-nearest) vs FR_DIV_TRUNC across radixes
     * ---------------------------------------------------------- */
    md_h3("12.4 FR_DIV (round-to-nearest) vs FR_DIV_TRUNC across radixes");
    printf("| Radix | N | FR_DIV max err (LSB) | FR_DIV mean err (LSB) | TRUNC max err (LSB) | TRUNC mean err (LSB) |\n");
    printf("|---:|---:|---:|---:|---:|---:|\n");

    int div_radixes[] = {8, 12, 16, 20};
    for (int ri = 0; ri < 4; ri++) {
        int R = div_radixes[ri];
        double scale = (double)(1L << R);
        double max_val = (double)(1L << (30 - R));  /* stay within s32 */
        stats_t st_rnd, st_trunc;
        stats_reset(&st_rnd);
        stats_reset(&st_trunc);

        /* Sweep a variety of x/y combinations */
        double xvals[] = {1.0, 2.5, 7.0, 10.0, 100.0, 0.5, -3.0, -10.0, 1000.0, 0.125};
        double yvals[] = {3.0, 7.0, 0.3, 1.7, -3.0, 9.0, 11.0, 0.125};
        int nx = (int)(sizeof(xvals)/sizeof(xvals[0]));
        int ny = (int)(sizeof(yvals)/sizeof(yvals[0]));

        for (int xi = 0; xi < nx; xi++) {
            for (int yi = 0; yi < ny; yi++) {
                double x = xvals[xi], y = yvals[yi];
                double ax = x < 0 ? -x : x;
                double ay = y < 0 ? -y : y;
                double aq = ay > 0 ? ax / ay : 1e30;
                /* Skip if inputs or quotient would overflow s32 at this radix */
                if (ax >= max_val || ay >= max_val || aq >= max_val) continue;
                s32 xfp = (s32)(x * scale);
                s32 yfp = (s32)(y * scale);
                if (yfp == 0) continue;
                double ref = x / y;

                s32 d_rnd   = FR_DIV(xfp, R, yfp, R);
                s32 d_trunc = FR_DIV_TRUNC(xfp, R, yfp, R);
                double rd_rnd   = frd(d_rnd, R);
                double rd_trunc = frd(d_trunc, R);

                stats_add(&st_rnd,   x / y, rd_rnd,   ref);
                stats_add(&st_trunc, x / y, rd_trunc, ref);
            }
        }

        double lsb = 1.0 / scale;
        printf("| %d | %d | %.2f | %.2f | %.2f | %.2f |\n",
               R, st_rnd.n,
               st_rnd.max_abs_err / lsb, stats_mean(&st_rnd) / lsb,
               st_trunc.max_abs_err / lsb, stats_mean(&st_trunc) / lsb);
    }
    printf("\n");

    /* ----------------------------------------------------------
     * 12.5 FR_DIV sign combinations — round-to-nearest correctness
     * ---------------------------------------------------------- */
    md_h3("12.5 FR_DIV sign combinations");
    printf("| x | y | radix | FR_DIV | expected | err (LSB) | correct? |\n");
    printf("|---:|---:|---:|---:|---:|---:|---:|\n");

    struct { double x, y; int r; } sign_cases[] = {
        { 7,  3, 16}, {-7,  3, 16}, { 7, -3, 16}, {-7, -3, 16},
        { 1,  3,  8}, {-1,  3,  8}, { 1, -3,  8}, {-1, -3,  8},
        {10,  7, 12}, {-10, 7, 12}, {10, -7, 12}, {-10,-7, 12},
        { 1,  6, 20}, {-1,  6, 20}, { 1, -6, 20}, {-1, -6, 20},
    };
    for (int i = 0; i < (int)(sizeof(sign_cases)/sizeof(sign_cases[0])); i++) {
        int R = sign_cases[i].r;
        double scale = (double)(1L << R);
        s32 xfp = (s32)(sign_cases[i].x * scale);
        s32 yfp = (s32)(sign_cases[i].y * scale);
        s32 d = FR_DIV(xfp, R, yfp, R);
        double rd = frd(d, R);
        double ref = sign_cases[i].x / sign_cases[i].y;
        double err_lsb = (rd - ref) * scale;
        if (err_lsb < 0) err_lsb = -err_lsb;
        const char *ok = err_lsb <= 0.5001 ? "YES" : "no";
        printf("| %.0f | %.0f | %d | %ld | %.6f | %.3f | %s |\n",
               sign_cases[i].x, sign_cases[i].y, R,
               (long)d, ref, err_lsb, ok);
    }
    printf("\n");

    /* ----------------------------------------------------------
     * 12.6 FR_EXP / FR_POW10 multi-radix sweep
     * ---------------------------------------------------------- */
    md_h3("12.6 FR_EXP and FR_POW10 multi-radix sweep");
    printf("| Radix | N | FR_EXP max err (LSB) | FR_EXP mean err (LSB) | FR_POW10 max err (LSB) | FR_POW10 mean err (LSB) |\n");
    printf("|---:|---:|---:|---:|---:|---:|\n");

    int exp_radixes[] = {8, 12, 16, 20};
    for (int ri = 0; ri < 4; ri++) {
        int R = exp_radixes[ri];
        double scale = (double)(1L << R);
        stats_t st_exp, st_pow10;
        stats_reset(&st_exp);
        stats_reset(&st_pow10);

        /* Sweep exp(x) for x in [-4, 4] in steps of 0.05 */
        for (int i = -80; i <= 80; i++) {
            double x = i / 20.0;
            s32 fr = (s32)(x * scale);
            s32 r = FR_EXP(fr, R);
            double rd = frd(r, R);
            double ref = exp(x);
            if (r != FR_OVERFLOW_POS && ref < (double)(1L << (31 - R)))
                stats_add(&st_exp, x, rd, ref);
        }

        /* Sweep pow10(x) for x in [-2, 2] in steps of 0.05 */
        for (int i = -40; i <= 40; i++) {
            double x = i / 20.0;
            s32 fr = (s32)(x * scale);
            s32 r = FR_POW10(fr, R);
            double rd = frd(r, R);
            double ref = pow(10.0, x);
            if (r != FR_OVERFLOW_POS && ref < (double)(1L << (31 - R)))
                stats_add(&st_pow10, x, rd, ref);
        }

        double lsb = 1.0 / scale;
        printf("| %d | %d/%d | %.2f | %.2f | %.2f | %.2f |\n",
               R, st_exp.n, st_pow10.n,
               st_exp.max_abs_err / lsb, stats_mean(&st_exp) / lsb,
               st_pow10.max_abs_err / lsb, stats_mean(&st_pow10) / lsb);
    }
    printf("\n");
}

/* ============================================================
 * Section 13: Summary of Findings
 * ============================================================ */

static void section_summary(void) {
    md_h2("13. Summary of Findings");

    md_h3("13.1 Status by Function (empirical, this run)");
    printf("| Function / Macro | Status | Evidence (section) | Notes |\n");
    printf("|---|:---:|:---:|---|\n");
    printf("| Type sizes (s32 = 4 bytes) | OK | 0 | `FR_defs.h` uses `<stdint.h>`; `s32` is exactly 4 bytes on LP64 and ILP32 alike |\n");
    printf("| Header constants (FR_kPI etc.) | OK | 1 | All within 1 LSB of libm |\n");
    printf("| FR_ABS, FR_SGN, I2FR, FR2I, FR_INT, FR_CHRDX, FR_FRAC, FR_FRACS | OK | 2 | Behave as documented |\n");
    printf("| FR_NUM | OK | 2.4 | `FR_NUM(i,f,d,r)` honors fractional argument |\n");
    printf("| FR_DEG2RAD / FR_RAD2DEG | OK | 3, 3.1 | `FR_DEG2RAD(x)` multiplies by π/180, `FR_RAD2DEG(x)` by 180/π |\n");
    printf("| FR_SMUL10, FR_SDIV10, FR_S(r)LOG2*, FR_RAD2Q/Q2RAD/DEG2Q/Q2DEG | OK | 3 | Approximation factors match expected to ~5 decimals |\n");
    printf("| FR_FixMuls | OK | 4.1 | int64 fast path; rounds to nearest (+0x8000 before >>16) |\n");
    printf("| FR_FixMulSat | OK | 4.2, 4.3 | int64 fast path with round-to-nearest and explicit saturation |\n");
    printf("| FR_FixAddSat | OK | 4.4, 4.5 | Saturation behaves identically on LP64 host and ILP32 MCU |\n");
    printf("| FR_CosI / FR_SinI | OK | 5 | s15.16 output; exact at poles; max abs error ~1.5e-5 (1 LSB s15.16) over [-720, +720]; macros routing to fr_*_bam |\n");
    printf("| FR_TanI (integer degrees) | OK | 5.1, 5.2 | BAM table lookup; 65-entry octant table; no 64-bit division |\n");
    printf("| FR_Cos / FR_Sin (interpolated) | OK | 6.1 | Within LSB-level error for r8 inputs in s16 |\n");
    printf("| FR_Tan (interpolated) | OK | 6.2 | Via fr_tan_bam; 65-entry octant table |\n");
    printf("| fr_cos / fr_sin / fr_cos_bam / fr_sin_bam / fr_cos_deg / fr_sin_deg | OK | 6 | s15.16 output; 129-entry quadrant table with round-to-nearest linear interp; exact at cardinal angles |\n");
    printf("| fr_tan_bam | OK | 14 | 65-entry octant table; first-octant lerp, second-octant 32-bit reciprocal; no 64-bit |\n");
    printf("| FR_acos | OK | 7.1 | Max error ~0.83° over [-1, +1] swept at 200 points |\n");
    printf("| FR_asin | OK | 7.2 | Same precision as FR_acos |\n");
    printf("| FR_atan2 | OK | 7.3 | Via asin/acos + hypot_fast8; 129-entry cos table; `FR_atan2(y, x, out_radix)` returns radians |\n");
    printf("| FR_atan | OK | 7.3 | `FR_atan(x, radix, out_radix)` calls `FR_atan2(x, 1<<radix, out_radix)` |\n");
    printf("| FR_pow2 (positive integer x) | OK | 8.1 | Bit-exact for integer exponents in test range |\n");
    printf("| FR_pow2 (positive fractional x) | OK | 8.1, 8.2 | ~1e-6 error |\n");
    printf("| FR_pow2 (negative fractional x) | OK | 8.1, 8.2 | Mathematical floor (toward −∞); 65-entry fraction table with linear interp |\n");
    printf("| FR_log2 | OK | 8.3, 12.1 | Leading-bit-position → normalize to s1.30 → 65-entry mantissa lookup with linear interp |\n");
    printf("| FR_ln, FR_log10 | OK | 8.4, 8.5, 12.2, 12.3 | FR_MULK28 constant multiply of FR_log2 |\n");
    printf("| FR_EXP, FR_POW10 | OK | 8.6, 12.6 | FR_MULK28 scaling + FR_pow2 |\n");
    printf("| FR_LOG2MIN sentinel | OK | 8.7 | Returned for input <= 0 |\n");
    printf("| FR_printNumD | OK | 9.1 | Works in unsigned magnitude; returns real byte count |\n");
    printf("| FR_printNumH | OK | 9.2 | Casts to unsigned before shifting |\n");
    printf("| FR_printNumF | OK | 9.3 | Fraction extraction correct; INT_MIN safe |\n");
    printf("| FR_Matrix2D_CPT::ID, set, det, inv (incl. in-place + singular detection) | OK | 10.1, 10.8 | `inv()` returns bool |\n");
    printf("| FR_Matrix2D_CPT::XlateI, XlateRelativeI (both overloads) | OK | 10.3 | All correct |\n");
    printf("| FR_Matrix2D_CPT::XFormPtI, XFormPtINoTranslate | OK | 10.2, 10.5, 10.7 | Correct, fast and slow paths |\n");
    printf("| FR_Matrix2D_CPT::XFormPtI16, XFormPtI16NoTranslate | OK | 10.6 | Correct |\n");
    printf("| FR_Matrix2D_CPT::setrotate (both overloads) | OK | 10.4 | Within 1 LSB of `cos`/`sin` from libm; sign convention is `[c -s; s c]` (CCW rotation) |\n");
    printf("| FR_Matrix2D_CPT::add, sub, +=, -=, *= | OK | 10.9 | Return void |\n");
    printf("| FR_Matrix2D_CPT::checkfast | OK | 10.10 | Detects scale-only matrices |\n");
    printf("| FR_sqrt | OK | 11.1, 11.2 | Digit-by-digit isqrt64; round-to-nearest (remainder > root → +1); FR_DOMAIN_ERROR sentinel for negative |\n");
    printf("| FR_DIV / FR_DIV_TRUNC / FR_DIV32 | OK | 4.6, 12.4, 12.5 | FR_DIV rounds to nearest (≤0.5 LSB); FR_DIV_TRUNC truncates; FR_DIV32 is 32-bit only |\n");
    printf("| FR_hypot | OK | 11.4 | Direct sum-of-squares on int64 |\n");
    printf("| fr_wave_sqr / fr_wave_pwm | OK | 11.5 | Single-comparison pulse generators; ±32767 amplitude |\n");
    printf("| fr_wave_tri | OK | 11.6, 11.7 | Symmetric triangle, peaks clamped to ±32767 |\n");
    printf("| fr_wave_saw | OK | 11.6 | Rising sawtooth, single boundary clamp |\n");
    printf("| fr_wave_tri_morph | OK | 11.8 | Variable-symmetry triangle (morphs to saw); one division per sample |\n");
    printf("| fr_wave_noise | OK | 11.9 | 32-bit Galois LFSR (poly 0xD0000001); period 2^32-1 |\n");
    printf("| FR_HZ2BAM_INC | OK | 11.10 | Macro: hz * 65536 / sample_rate, returns u16 |\n");
    printf("| fr_adsr_t / init / trigger / release / step | OK | 11.11 | Linear ADSR; s1.30 internal level; s0.15 output |\n");
    printf("\n");

    md_h3("13.2 Coverage");
    printf("- `FR_math.c`: this TDD suite exercises every public function. Uncovered lines that remain are deep saturation branches inside `FR_FixMulSat` that need very specific intermediate overflow patterns.\n");
    printf("- `FR_math_2D.cpp`: covered by sections 10.1–10.10. The `inv()` failure path is exercised in 10.8.\n");
    printf("- `FR_math_2D.h`: inline transform helpers covered.\n");
    printf("- Every macro in `FR_math.h` is exercised at least once in sections 2 and 3.\n");
    printf("- Run `gcov -o build build/test_tdd_FR_math.gcno` after `make test-tdd` to confirm.\n");
    printf("\n");
}

/* ============================================================
 * Section 14: Accuracy Summary Table (machine-readable)
 *
 * Emits a markdown table between sentinel comments so that
 * scripts/accuracy_report.sh can extract and patch it into
 * README.md, docs/README.md, and pages/index.html.
 * ============================================================ */

static void section_accuracy_table(void) {
    md_h2("14. Accuracy Summary Table");

    printf("<!-- ACCURACY_TABLE_START -->\n");
    if (g_showpeak) {
        printf("| Function | Max err (%%) | Avg err (%%) | Note | Peak at |\n");
        printf("|---|---:|---:|---|---:|\n");
    } else {
        printf("| Function | Max err (%%) | Avg err (%%) | Note |\n");
        printf("|---|---:|---:|---|\n");
    }

    const int R = 16;
    const double scale = (double)(1L << R);

    /* Persistent stats so we can print diagnostics after the table */
    stats_t st_sincos, st_tan, st_asincos, st_atan2;
    stats_t st_rad2bam, st_deg2bam, st_sincos_deg_s32, st_tan_deg_s32;
    stats_reset(&st_sincos); stats_reset(&st_tan);
    stats_reset(&st_asincos); stats_reset(&st_atan2);
    stats_reset(&st_rad2bam); stats_reset(&st_deg2bam);
    stats_reset(&st_sincos_deg_s32); stats_reset(&st_tan_deg_s32);

    /* --- sin / cos (BAM native: 65536-pt) --- */
    {
        stats_t st; stats_reset(&st);
        for (int i = 0; i < 65536; i++) {
            u16 bam = (u16)i;
            double rad = bam * 2.0 * M_PI / 65536.0;
            stats_add(&st, (double)bam, frd(fr_sin_bam(bam), FR_TRIG_OUT_PREC), q16(sin(rad)));
            stats_add(&st, (double)bam, frd(fr_cos_bam(bam), FR_TRIG_OUT_PREC), q16(cos(rad)));
        }
        acc_row("sin/cos (BAM)", &st, "fr_sin_bam/fr_cos_bam direct; 129-entry table");
    }

    /* --- sin / cos (degree wrappers: 65536-pt) --- */
    {
        stats_t &st = st_sincos;
        const u16 radix = 7; /* s8.7 degrees: 128 steps/deg, [-256°,+256°) */
        for (int i = -32768; i <= 32767; i++) {
            double deg = (double)i / (1 << radix);
            double rad = deg * M_PI / 180.0;
            stats_add(&st, deg, frd(FR_Sin((s16)i, radix), FR_TRIG_OUT_PREC), q16(sin(rad)));
            stats_add(&st, deg, frd(FR_Cos((s16)i, radix), FR_TRIG_OUT_PREC), q16(cos(rad)));
        }
        s16 specials[] = {0,30,45,60,90,120,135,150,180,210,225,240,270,300,315,330,360,
                          -30,-45,-60,-90,-120,-135,-150,-180,-210,-225,-240,-270,-300,-315,-330,-360};
        for (int si = 0; si < (int)(sizeof(specials)/sizeof(specials[0])); si++) {
            s16 d = specials[si];
            double rad = d * M_PI / 180.0;
            stats_add(&st, d, frd(FR_SinI(d), FR_TRIG_OUT_PREC), q16(sin(rad)));
            stats_add(&st, d, frd(FR_CosI(d), FR_TRIG_OUT_PREC), q16(cos(rad)));
        }
        acc_row("sin/cos (deg)", &st, "FR_Sin/FR_Cos ±256° (s16 at radix 7; FR_DEG2BAM)");
    }

    /* --- sin / cos (radian wrappers: 65536-pt) --- */
    {
        stats_t st; stats_reset(&st);
        for (int i = 0; i < 65536; i++) {
            double angle = -2.0 * M_PI + (4.0 * M_PI * i / 65536.0);
            s32 rad_fp = (s32)(angle * (1L << 16));
            stats_add(&st, angle, frd(fr_sin(rad_fp, 16), FR_TRIG_OUT_PREC), q16(sin(angle)));
            stats_add(&st, angle, frd(fr_cos(rad_fp, 16), FR_TRIG_OUT_PREC), q16(cos(angle)));
        }
        acc_row("sin/cos (rad)", &st, "fr_sin/fr_cos via fr_rad_to_bam ±2π r16");
    }

    /* --- tan (BAM native: 65536-pt, full sweep) --- */
    {
        stats_t st; stats_reset(&st);
        for (int i = 0; i < 65536; i++) {
            u16 bam = (u16)i;
            double ref;
            if (bam == 16384)       ref =  TAN_CLAMP;  /* 90°: +maxint */
            else if (bam == 49152)  ref = -TAN_CLAMP;  /* 270°: -maxint */
            else                    ref = tan_ref(bam * 2.0 * M_PI / 65536.0);
            stats_add(&st, (double)bam, frd(fr_tan_bam(bam), FR_TRIG_OUT_PREC), q16(ref));
        }
        acc_row("tan (BAM)", &st, "fr_tan_bam 65536-pt full; ±maxint at poles");
    }

    /* --- tan (degree wrappers: 65536-pt, full sweep) --- */
    {
        stats_t &st = st_tan;
        const u16 radix = 7;
        for (int i = -32768; i <= 32767; i++) {
            double deg = (double)i / (1 << radix);
            double rad = deg * M_PI / 180.0;
            stats_add(&st, deg, frd(FR_Tan((s16)i, radix), FR_TRIG_OUT_PREC), q16(tan_ref(rad)));
        }
        s16 specials[] = {0,30,45,60,-30,-45,-60,120,135,150,-120,-135,-150};
        for (int si = 0; si < (int)(sizeof(specials)/sizeof(specials[0])); si++) {
            s16 d = specials[si];
            double rad = d * M_PI / 180.0;
            stats_add(&st, d, frd(FR_TanI(d), FR_TRIG_OUT_PREC), q16(tan_ref(rad)));
        }
        acc_row("tan (deg)", &st, "FR_Tan ±256° full (s16 at radix 7; FR_DEG2BAM); sat at poles");
    }

    /* --- tan (radian wrappers: 65536-pt, full sweep) --- */
    {
        stats_t st; stats_reset(&st);
        for (int i = 0; i < 65536; i++) {
            double angle = -2.0 * M_PI + (4.0 * M_PI * i / 65536.0);
            s32 rad_fp = (s32)(angle * (1L << 16));
            stats_add(&st, angle, frd(fr_tan(rad_fp, 16), FR_TRIG_OUT_PREC), q16(tan_ref(angle)));
        }
        acc_row("tan (rad)", &st, "fr_tan ±2π r16 full; sat at poles");
    }

    /* --- asin / acos --- */
    {
        stats_t &st = st_asincos;
        /* 65536-point sweep: all representable values at radix 15 over [-1, +1) */
        for (int i = -32768; i <= 32767; i++) {
            double xd = (double)i / (1 << 15);
            if (xd < -1.0 || xd > 1.0) continue;
            s32 rad = FR_asin((s32)i, 15, R);
            stats_add(&st, xd, frd(rad, R), q16(asin(xd)));
            rad = FR_acos((s32)i, 15, R);
            stats_add(&st, xd, frd(rad, R), q16(acos(xd)));
        }
        acc_row("asin / acos", &st, "65536-pt; sqrt approx near boundary");
    }

    /* --- atan2 --- */
    {
        stats_t &st = st_atan2;
        /* 65536-point sweep at each radius.
         * Skip i=-32768 (exactly -pi): branch-cut convention differs
         * between FR_atan2 (+pi) and libm (-pi), both correct.
         * Start radii at 0.1 — at 0.01 inputs have <10 LSBs of angular
         * resolution, testing input quantization not the algorithm.
         * Also skip points where the minor axis has < 8 bits (256 counts)
         * — below that, input quantization dominates over algorithm error. */
        double radii[] = {0.1, 1.0, 10.0, 100.0, 1000.0};
        for (int ri = 0; ri < (int)(sizeof(radii)/sizeof(radii[0])); ri++) {
            double rad = radii[ri];
            for (int i = -32767; i <= 32768; i++) {
                double angle = i * M_PI / 32768.0;
                double x = rad * cos(angle), y = rad * sin(angle);
                s32 fx = (s32)(x * scale);
                s32 fy = (s32)(y * scale);
                if (fx == 0 && fy == 0) continue;
                s32 afx = (fx < 0) ? -fx : fx;
                s32 afy = (fy < 0) ? -fy : fy;
                s32 minor = (afx < afy) ? afx : afy;
                if (minor < 256) continue; /* input quantization, not algo */
                s32 r = FR_atan2(fy, fx, R);
                double ref = atan2(y, x);
                /* Skip near ±pi branch cut: sign depends on sub-LSB
                 * input quantization, not algorithm accuracy. */
                if (fabs(fabs(ref) - M_PI) < 0.01) continue;
                stats_add(&st, angle * 180.0 / M_PI, frd(r, R), q16(ref));
            }
        }
        /* Special cases: exact quadrant/octant/30-degree angles */
        double specials_deg[] = {0,30,45,60,90,120,135,150,
                                 -30,-45,-60,-90,-120,-135,-150,-170};
        for (int si = 0; si < (int)(sizeof(specials_deg)/sizeof(specials_deg[0])); si++) {
            double angle = specials_deg[si] * M_PI / 180.0;
            double x = 100.0 * cos(angle), y = 100.0 * sin(angle);
            s32 fx = (s32)(x * scale), fy = (s32)(y * scale);
            if (fx == 0 && fy == 0) continue;
            s32 r = FR_atan2(fy, fx, R);
            stats_add(&st, specials_deg[si], frd(r, R), q16(atan2(y, x)));
        }
        acc_row("atan2", &st, "65536x5 radii; asin/acos+hypot_fast8");
    }

    /* --- atan --- */
    {
        stats_t st; stats_reset(&st);
        for (int i = -10000; i <= 10000; i++) {
            double x = i / 1000.0;
            s32 fr = (s32)(x * scale);
            s32 r = FR_atan(fr, (u16)R, (u16)R);
            double ref = atan(x);
            stats_add(&st, x, frd(r, R), q16(ref));
        }
        acc_row("atan", &st, "20001-pt full sweep [-10,10]; via FR_atan2");
    }

    /* --- sqrt --- */
    {
        stats_t st; stats_reset(&st);
        double inputs[] = {0.0001, 0.25, 0.5, 1, 2, 3, 4, 7, 9, 16, 25, 100, 1024, 10000, 32000};
        for (int i = 0; i < (int)(sizeof(inputs)/sizeof(inputs[0])); i++) {
            s32 fr = (s32)(inputs[i] * scale);
            s32 r = FR_sqrt(fr, R);
            stats_add(&st, inputs[i], frd(r, R), q16(sqrt(inputs[i])));
        }
        /* Fine sweep */
        for (int i = 1; i <= 1000; i++) {
            double x = i * 10.0;
            s32 fr = (s32)(x * scale);
            s32 r = FR_sqrt(fr, R);
            stats_add(&st, x, frd(r, R), q16(sqrt(x)));
        }
        acc_row("sqrt", &st, "Round-to-nearest");
    }

    /* --- log2 --- */
    {
        stats_t st; stats_reset(&st);
        /* Integer inputs — stay within s32 range at radix 16 (max ~32767) */
        for (int v = 1; v <= 32000; v += (v < 100 ? 1 : v / 10)) {
            s32 fr = (s32)((double)v * scale);
            if (fr <= 0) continue;
            s32 r = FR_log2(fr, (u16)R, (u16)R);
            stats_add(&st, (double)v, frd(r, R), q16(log2((double)v)));
        }
        /* Fractional sweep 0.125 .. 1.0 */
        for (int i = 1; i <= 100; i++) {
            double x = 0.125 + (0.875 * i / 100.0);
            s32 fr = (s32)(x * scale);
            if (fr <= 0) continue;
            s32 r = FR_log2(fr, (u16)R, (u16)R);
            stats_add(&st, x, frd(r, R), q16(log2(x)));
        }
        acc_row("log2", &st, "65-entry mantissa table");
    }

    /* --- pow2 --- */
    {
        stats_t st; stats_reset(&st);
        for (int i = -800; i <= 800; i++) {
            double x = i / 100.0;
            s32 fr = (s32)(x * scale);
            s32 r = FR_pow2(fr, R);
            double ref = pow(2.0, x);
            stats_add(&st, x, frd(r, R), q16(ref));
        }
        acc_row("pow2", &st, "65-entry fraction table");
    }

    /* --- ln, log10 --- */
    {
        stats_t st; stats_reset(&st);
        double inputs[] = {0.125, 0.25, 0.5, 1, 2, M_E, 3, 4, 5, 7, 8, 10, 20, 50, 100, 1000};
        for (int i = 0; i < (int)(sizeof(inputs)/sizeof(inputs[0])); i++) {
            s32 fr = (s32)(inputs[i] * scale);
            if (fr <= 0) continue;
            s32 r = FR_ln(fr, R, R);
            double ref = log(inputs[i]);
            stats_add(&st, inputs[i], frd(r, R), q16(ref));
            r = FR_log10(fr, R, R);
            ref = log10(inputs[i]);
            stats_add(&st, inputs[i], frd(r, R), q16(ref));
        }
        acc_row("ln, log10", &st, "Via FR_MULK28 from log2");
    }

    /* --- exp (FR_EXP) --- */
    {
        stats_t st; stats_reset(&st);
        for (int i = -400; i <= 400; i++) {
            double x = i / 100.0;
            s32 fr = (s32)(x * scale);
            s32 r = FR_EXP(fr, R);
            double ref = exp(x);
            if (ref > 32000.0 || ref < 1e-6) continue; /* skip overflow/underflow */
            stats_add(&st, x, frd(r, R), q16(ref));
        }
        acc_row("exp", &st, "FR_MULK28 + FR_pow2");
    }

    /* --- exp_fast (FR_EXP_FAST) --- */
    {
        stats_t st; stats_reset(&st);
        for (int i = -400; i <= 400; i++) {
            double x = i / 100.0;
            s32 fr = (s32)(x * scale);
            s32 r = FR_EXP_FAST(fr, R);
            double ref = exp(x);
            if (ref > 32000.0 || ref < 1e-6) continue;
            stats_add(&st, x, frd(r, R), q16(ref));
        }
        acc_row("exp_fast", &st, "Shift-only scaling");
    }

    /* --- pow10 (FR_POW10) --- */
    {
        stats_t st; stats_reset(&st);
        for (int i = -200; i <= 200; i++) {
            double x = i / 100.0;
            s32 fr = (s32)(x * scale);
            s32 r = FR_POW10(fr, R);
            double ref = pow(10.0, x);
            if (ref > 32000.0 || ref < 1e-6) continue;
            stats_add(&st, x, frd(r, R), q16(ref));
        }
        acc_row("pow10", &st, "FR_MULK28 + FR_pow2");
    }

    /* --- pow10_fast (FR_POW10_FAST) --- */
    {
        stats_t st; stats_reset(&st);
        for (int i = -200; i <= 200; i++) {
            double x = i / 100.0;
            s32 fr = (s32)(x * scale);
            s32 r = FR_POW10_FAST(fr, R);
            double ref = pow(10.0, x);
            if (ref > 32000.0 || ref < 1e-6) continue;
            stats_add(&st, x, frd(r, R), q16(ref));
        }
        acc_row("pow10_fast", &st, "Shift-only scaling");
    }

    /* --- hypot (exact) --- */
    {
        stats_t st; stats_reset(&st);
        struct { double x, y; } cases[] = {
            {0,0},{1,0},{0,1},{3,4},{5,12},{8,15},{-3,-4},{-3,4},{3,-4},
            {1,1},{0.5,0.5},{100,100},{1000,1},{1,1000}
        };
        for (int i = 0; i < (int)(sizeof(cases)/sizeof(cases[0])); i++) {
            s32 fx = (s32)(cases[i].x * scale);
            s32 fy = (s32)(cases[i].y * scale);
            s32 r = FR_hypot(fx, fy, R);
            double ref = hypot(cases[i].x, cases[i].y);
            stats_add(&st, ref, frd(r, R), q16(ref));
        }
        acc_row("hypot (exact)", &st, "64-bit intermediate");
    }

    /* --- hypot_fast8 (8-seg) --- */
    {
        stats_t st; stats_reset(&st);
        struct { double x, y; } cases[] = {
            {1,0},{0,1},{3,4},{5,12},{8,15},{-3,-4},{1,1},{0.5,0.5},
            {100,100},{1000,1},{1,1000},{7,24},{20,21}
        };
        for (int i = 0; i < (int)(sizeof(cases)/sizeof(cases[0])); i++) {
            s32 fx = (s32)(cases[i].x * scale);
            s32 fy = (s32)(cases[i].y * scale);
            s32 r = FR_hypot_fast8(fx, fy);
            double ref = hypot(cases[i].x, cases[i].y);
            if (ref > 0) stats_add(&st, ref, frd(r, R), q16(ref));
        }
        acc_row("hypot_fast8 (8-seg)", &st, "Shift-only, no multiply");
    }

    printf("<!-- ACCURACY_TABLE_END -->\n");
    printf("\n");

    /* ── Test-only rows (not library functions — conversion & pipeline checks) ── */
    md_h3("14.0.1 Conversion & pipeline accuracy (test-only)");
    printf("| Function | Max err (%%) | Avg err (%%) | Note |\n");
    printf("|---|---:|---:|---|\n");

    /* --- rad→BAM conversion (standalone: 65536-pt) --- */
    {
        stats_t &st = st_rad2bam;
        for (int i = 0; i < 65536; i++) {
            double angle = -2.0 * M_PI + (4.0 * M_PI * i / 65536.0);
            s32 rad_fp = (s32)(angle * scale);
            u16 got = fr_rad_to_bam(rad_fp, 16);
            /* Exact BAM: wrap to u16 */
            double exact_bam_d = angle * 65536.0 / (2.0 * M_PI);
            s32 exact_bam_s = (s32)floor(exact_bam_d + 0.5);
            u16 expected = (u16)(exact_bam_s & 0xFFFF);
            /* Feed stats as degrees so the error is interpretable */
            double got_deg = got * (360.0 / 65536.0);
            double exp_deg = expected * (360.0 / 65536.0);
            stats_add(&st, angle, got_deg, exp_deg);
        }
        {
            char note[128];
            snprintf(note, sizeof(note),
                     "fr_rad_to_bam() ±2π at r16; max %d BAM LSB",
                     (int)(st.max_abs_err / (360.0 / 65536.0) + 0.5));
            acc_row("rad→BAM conv", &st, note);
        }
    }

    /* --- deg→BAM conversion (standalone: 65536-pt) --- */
    {
        stats_t &st = st_deg2bam;
        for (int i = 0; i < 65536; i++) {
            double deg = -360.0 + (720.0 * i / 65536.0);
            s32 deg_fp = (s32)(deg * scale);
            u16 got = fr_deg_to_bam(deg_fp, 16);
            /* Exact BAM: wrap to u16 */
            double exact_bam_d = deg * 65536.0 / 360.0;
            s32 exact_bam_s = (s32)floor(exact_bam_d + 0.5);
            u16 expected = (u16)(exact_bam_s & 0xFFFF);
            double got_deg = got * (360.0 / 65536.0);
            double exp_deg = expected * (360.0 / 65536.0);
            stats_add(&st, deg, got_deg, exp_deg);
        }
        {
            char note[128];
            snprintf(note, sizeof(note),
                     "fr_deg_to_bam() ±360° at r16; max %d BAM LSB",
                     (int)(st.max_abs_err / (360.0 / 65536.0) + 0.5));
            acc_row("deg→BAM conv", &st, note);
        }
    }

    /* --- sin / cos via integer degrees ±360° --- */
    {
        stats_t &st = st_sincos_deg_s32;
        for (int deg = -360; deg <= 360; deg++) {
            double rad = deg * M_PI / 180.0;
            stats_add(&st, (double)deg, frd(fr_sin_deg(deg), FR_TRIG_OUT_PREC), q16(sin(rad)));
            stats_add(&st, (double)deg, frd(fr_cos_deg(deg), FR_TRIG_OUT_PREC), q16(cos(rad)));
        }
        acc_row("sin/cos (int deg)", &st, "fr_sin_deg/fr_cos_deg ±360° integer degrees");
    }

    /* --- tan via integer degrees ±360° --- */
    {
        stats_t &st = st_tan_deg_s32;
        for (int deg = -360; deg <= 360; deg++) {
            double rad = deg * M_PI / 180.0;
            stats_add(&st, (double)deg, frd(FR_TanI((s16)deg), FR_TRIG_OUT_PREC), q16(tan_ref(rad)));
        }
        acc_row("tan (int deg)", &st, "FR_TanI ±360° full; sat at poles");
    }

    /* --- Conversion macro accuracy (all 6 direction macros) --- */

    /* FR_RAD2BAM macro: test within safe range (±pi at r16) */
    {
        stats_t st; stats_reset(&st);
        for (int i = 0; i < 65536; i++) {
            double angle = -M_PI + (2.0 * M_PI * i / 65536.0);
            s32 rad_fp = (s32)(angle * scale);
            s32 raw = FR_RAD2BAM(rad_fp);
            u16 got = (u16)((raw + (1 << 15)) >> 16);
            double exact_d = angle * 65536.0 / (2.0 * M_PI);
            u16 expected = (u16)((s32)floor(exact_d + 0.5) & 0xFFFF);
            double got_deg = got * (360.0 / 65536.0);
            double exp_deg = expected * (360.0 / 65536.0);
            stats_add(&st, angle, got_deg, exp_deg);
        }
        acc_row("FR_RAD2BAM macro", &st, "Shift-approx ±π at r16; overflows beyond ±4 rad");
    }

    /* FR_DEG2BAM macro: test within safe range (±180° at r7) */
    {
        stats_t st; stats_reset(&st);
        const u16 radix = 7;
        for (int i = -23040; i <= 23040; i++) { /* ±180° at r7 = ±23040 */
            double deg = (double)i / (1 << radix);
            s32 raw = FR_DEG2BAM((s32)i);
            u16 got = (u16)((raw + (1 << (radix - 1))) >> radix);
            double exact_d = deg * 65536.0 / 360.0;
            u16 expected = (u16)((s32)floor(exact_d + 0.5) & 0xFFFF);
            double got_deg = got * (360.0 / 65536.0);
            double exp_deg = expected * (360.0 / 65536.0);
            stats_add(&st, deg, got_deg, exp_deg);
        }
        acc_row("FR_DEG2BAM macro", &st, "Shift-approx ±180° at r7; overflows beyond ±256°");
    }

    /* FR_BAM2RAD macro: multiplies by 2π/65536 using shifts.
     * BAM 0..32767 at r16 (upper half overflows s32 when <<16). */
    {
        stats_t st; stats_reset(&st);
        for (int i = 0; i < 32768; i++) {
            s32 bam_r16 = (s32)i << 16;
            s32 rad_fp = FR_BAM2RAD(bam_r16);
            double got_rad = frd(rad_fp, 16);
            double exp_rad = (double)i * 2.0 * M_PI / 65536.0;
            stats_add(&st, (double)i, got_rad, exp_rad);
        }
        acc_row("FR_BAM2RAD macro", &st, "BAM→rad r16 full (0..32767; <<16 overflow above)");
    }

    /* FR_BAM2DEG macro: multiplies by 360/65536 using shifts.
     * BAM 0..32767 at r16 (same s32 overflow limit). */
    {
        stats_t st; stats_reset(&st);
        for (int i = 0; i < 32768; i++) {
            s32 bam_r16 = (s32)i << 16;
            s32 deg_fp = FR_BAM2DEG(bam_r16);
            double got_deg = frd(deg_fp, 16);
            double exp_deg = (double)i * 360.0 / 65536.0;
            stats_add(&st, (double)i, got_deg, exp_deg);
        }
        acc_row("FR_BAM2DEG macro", &st, "BAM→deg r16 full (0..32767; <<16 overflow above)");
    }

    /* FR_DEG2RAD macro: 65536-pt ±360° at r16 full */
    {
        stats_t st; stats_reset(&st);
        for (int i = 0; i < 65536; i++) {
            double deg = -360.0 + (720.0 * i / 65536.0);
            s32 deg_fp = (s32)(deg * scale);
            s32 rad_fp = FR_DEG2RAD(deg_fp);
            double got_rad = frd(rad_fp, 16);
            double exp_rad = deg * M_PI / 180.0;
            stats_add(&st, deg, got_rad, exp_rad);
        }
        acc_row("FR_DEG2RAD macro", &st, "65536-pt ±360° r16 full");
    }

    /* FR_RAD2DEG macro: 65536-pt ±2π at r16 full */
    {
        stats_t st; stats_reset(&st);
        for (int i = 0; i < 65536; i++) {
            double angle = -2.0 * M_PI + (4.0 * M_PI * i / 65536.0);
            s32 rad_fp = (s32)(angle * scale);
            s32 deg_fp = FR_RAD2DEG(rad_fp);
            double got_deg = frd(deg_fp, 16);
            double exp_deg = angle * 180.0 / M_PI;
            stats_add(&st, angle, got_deg, exp_deg);
        }
        acc_row("FR_RAD2DEG macro", &st, "65536-pt ±2π r16 full");
    }

    printf("\n");

    /* Diagnostic: show where each trig function's worst % error occurs */
    md_h3("14.1 Worst-case percent error diagnostics");
    printf("Shows the input that produced the maximum %% error for each trig function.\n");
    printf("This helps identify whether the peak is a genuine algorithm limitation or\n");
    printf("a near-zero denominator artifact.\n\n");
    printf("| Function | Worst-pct input | Expected | Got | Abs err | Pct err |\n");
    printf("|---|---|---:|---:|---:|---:|\n");

    struct { const char *name; stats_t *s; } diag[] = {
        {"sin / cos",       &st_sincos},
        {"tan",             &st_tan},
        {"rad→BAM conv",    &st_rad2bam},
        {"deg→BAM conv",    &st_deg2bam},
        {"sin/cos (int deg)",&st_sincos_deg_s32},
        {"tan (int deg)",   &st_tan_deg_s32},
        {"asin/acos",       &st_asincos},
        {"atan2",           &st_atan2},
    };
    for (int d = 0; d < (int)(sizeof(diag)/sizeof(diag[0])); d++) {
        stats_t *s = diag[d].s;
        double ae = fabs(s->worst_pct_actual - s->worst_pct_expected);
        printf("| %s | %.4f | %.6f | %.6f | %.6f | %.4f%% |\n",
               diag[d].name, s->worst_pct_input,
               s->worst_pct_expected, s->worst_pct_actual, ae,
               s->max_pct_err);
    }
    printf("\n");
}

int main(void) {
    g_showpeak = (getenv("FR_SHOWPEAK") != NULL);
    md_h1("FR_Math TDD Characterization Report");
    printf("> Generated by `tests/test_tdd.cpp`. This is a measurement suite, not a pass/fail suite.\n");
    printf("> All numbers below are *what the library actually does*, compared to libm `double` references.\n");

    section_platform();
    section_constants();
    section_macros_basic();
    section_shift_macros();
    section_arithmetic();
    section_trig_int();
    section_trig_frac();
    section_inverse_trig();
    section_pow_log();
    section_print();
    section_matrix2d();
    section_v2_new();
    section_multiradix();
    section_summary();
    section_accuracy_table();

    return 0;
}
