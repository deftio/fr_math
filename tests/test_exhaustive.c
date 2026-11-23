/* test_exhaustive.c - every-input accuracy tests for FR_Math.
 *
 * For every radix 0..16 each function is run on every input in its test
 * domain and compared with a double-precision reference computed from the
 * exact input value. Nothing in a domain is skipped: zero crossings,
 * poles, saturation and domain edges are all checked, each against the
 * documented convention for that case. The worst error per function and
 * radix is printed, and the test fails if any exceeds its budget.
 *
 * Domains, per function and radix r (raw s32 input values):
 *   BAM functions        every u16 (65536)
 *   radian functions     every raw value in the s16 range, every value in
 *                        [-4pi, 4pi], the 65536 values at each end of s32,
 *                        and every 65521st value across all of s32
 *   degree functions     the same, with [-720, 720] degrees
 *   other functions      see the domain comment at each check below
 * With --s32, the angle functions are also run on every s32 input at
 * radix 16 (4.3 billion each; minutes, not part of `make test`).
 *
 * Error measures:
 *   sin, cos             |got - exact| in s15.16 LSBs
 *   tan                  |got - exact| / max(|exact|, 1.0), in %; where
 *                        |exact| >= 32768 the result must be
 *                        +/-FR_TRIG_MAXVAL with the sign of the exact value
 *                        (or within the budget of the exact value, for
 *                        results just past the range); at an exact pole
 *                        it must be +/-FR_TRIG_MAXVAL
 *   angle -> BAM         circular distance to the exact BAM, in BAM units
 *   inverse trig, logs   |got - exact| in output LSBs (output radix 16)
 *   sqrt                 |got - exact| in output LSBs
 *   pow2, exp, pow10     |got - exact| / max(|exact|, 2^16 LSBs), in %;
 *                        where the exact result is >= 2^31 LSBs it must be
 *                        FR_OVERFLOW_POS
 *
 * Budgets (g_budgets) are the measured worst errors of this version
 * rounded up a little, so that any regression fails. They record what
 * the library does, not a claim that it is good enough: the printed
 * table shows every worst case and where it occurs.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include "FR_math.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define MAXV 2147483647.0

static int g_fail = 0;

/*---------------------------------------------------------------------
 * Budgets: worst error measured for this version (every input in the
 * domains below), x1.1 rounded up to two digits. Per radix where the
 * error depends on it: FR_EXP/FR_POW10 compute the base-2 exponent at the
 * input radix, so its rounding (2^-r) dominates at low radix; asin/acos
 * at radix 0 only see -1, 0, 1.
 */
typedef struct { const char *name; double all; double r[17]; } budget_t;
static const budget_t g_budgets[] = {
    {"fr_sin_bam", 3.2, {0}}, {"fr_cos_bam", 3.2, {0}}, {"fr_tan_bam", 0.017, {0}},
    {"fr_sin", 6.0, {0}}, {"fr_cos", 6.4, {0}}, {"fr_tan", 0.054, {0}},
    {"fr_sin_deg", 5.5, {0}}, {"fr_cos_deg", 5.8, {0}}, {"fr_tan_deg", 0.045, {0}},
    {"fr_rad_to_bam", 0.74, {0}}, {"fr_deg_to_bam", 0.60, {0}},
    {"FR_SinI", 4.8, {0}}, {"FR_CosI", 4.8, {0}}, {"FR_TanI", 0.031, {0}},
    {"FR_asin", 0, {0.78, 12, 12, 12, 15, 15, 19, 19, 22, 22, 29, 27, 32, 32, 32, 32, 32}},
    {"FR_acos", 0, {0.46, 12, 12, 12, 15, 15, 19, 19, 21, 21, 29, 26, 32, 32, 32, 32, 32}},
    {"FR_atan", 70, {0}}, {"FR_atan2", 71, {0}},
    {"FR_sqrt", 0.5, {0}}, {"FR_hypot", 0.5, {0}}, {"FR_hypot_fast8", 0.14, {0}},
    {"FR_log2", 4.4, {0}}, {"FR_ln", 3.6, {0}}, {"FR_log10", 1.9, {0}},
    {"FR_pow2", 0.0022, {0}},
    {"FR_EXP", 0, {43, 21, 9.4, 4.8, 2.4, 1.2, 0.60, 0.30, 0.16, 0.076, 0.039, 0.021, 0.012, 0.0068, 0.0046, 0.0035, 0.0028}},
    {"FR_POW10", 0, {38, 21, 8.7, 4.7, 2.5, 1.2, 0.60, 0.30, 0.16, 0.076, 0.039, 0.021, 0.012, 0.0069, 0.0046, 0.0033, 0.0028}},
    {"FR_EXP_FAST", 0, {82, 32, 14, 11, 7.8, 4.8, 3.2, 1.8, 1.1, 0.54, 0.32, 0.17, 0.087, 0.047, 0.026, 0.017, 0.012}},
    {"FR_POW10_FAST", 0, {64, 52, 32, 20, 13, 6.8, 3.9, 2.5, 1.3, 0.74, 0.42, 0.23, 0.12, 0.064, 0.038, 0.025, 0.017}},
};

static double budget_of(const char *name, int radix)
{
    size_t i;
    for (i = 0; i < sizeof g_budgets / sizeof g_budgets[0]; i++)
        if (!strcmp(g_budgets[i].name, name))
            return g_budgets[i].all > 0 ? g_budgets[i].all : g_budgets[i].r[radix < 0 ? 16 : radix];
    fprintf(stderr, "no budget for %s\n", name);
    exit(2);
}

/*---------------------------------------------------------------------
 * Error tracking: one row per (function, radix).
 */
typedef struct {
    const char *name;
    int radix;          /* -1: not radix dependent */
    double budget;
    const char *unit;
    long n;
    double worst;
    s32 worst_in, worst_in2;
    s32 worst_got;
    double worst_want;
    long conv_bad;      /* convention (saturation/pole/domain) violations */
    s32 conv_in, conv_in2, conv_got;
} track_t;

static void tr_init(track_t *t, const char *name, int radix, const char *unit)
{
    memset(t, 0, sizeof *t);
    t->name = name; t->radix = radix; t->budget = budget_of(name, radix); t->unit = unit;
}

static void tr_add(track_t *t, double err, s32 in, s32 in2, s32 got, double want)
{
    t->n++;
    if (err > t->worst) {
        t->worst = err; t->worst_in = in; t->worst_in2 = in2;
        t->worst_got = got; t->worst_want = want;
    }
}

static void tr_conv(track_t *t, int ok, s32 in, s32 in2, s32 got)
{
    t->n++;
    if (!ok) {
        if (t->conv_bad == 0) { t->conv_in = in; t->conv_in2 = in2; t->conv_got = got; }
        t->conv_bad++;
    }
}

static void tr_report(const track_t *t)
{
    int bad = (t->worst > t->budget) || t->conv_bad;
    char rad[16];
    if (t->radix < 0) strcpy(rad, "-"); else snprintf(rad, sizeof rad, "%d", t->radix);
    printf("| %-16s | %3s | %11ld | %10.4f %-4s | %8.4f | %11d %11d | %s\n",
           t->name, rad, t->n, t->worst, t->unit, t->budget,
           (int)t->worst_in, (int)t->worst_in2, bad ? "FAIL" : "ok");
    if (t->worst > t->budget)
        printf("    FAIL %s r%s: worst %.6f %s > budget %.6f at input %d %d: got %d, exact %.3f\n",
               t->name, rad, t->worst, t->unit, t->budget,
               (int)t->worst_in, (int)t->worst_in2, (int)t->worst_got, t->worst_want);
    if (t->conv_bad)
        printf("    FAIL %s r%s: %ld convention violations, first at input %d %d: got %d\n",
               t->name, rad, t->conv_bad, (int)t->conv_in, (int)t->conv_in2, (int)t->conv_got);
    if (bad) g_fail = 1;
}

static void table_header(const char *title)
{
    printf("\n### %s\n\n", title);
    printf("| %-16s | %3s | %11s | %15s | %8s | %23s |\n",
           "function", "r", "inputs", "worst error", "budget", "worst at input");
    printf("|------------------|-----|-------------|-----------------|----------|-------------------------|\n");
}

/*---------------------------------------------------------------------
 * Input domains. visit() is called once per input; duplicates between the
 * pieces of a domain are harmless.
 */
typedef void (*visit_fn)(s32 x, u16 radix, void *ctx);

/* s16 range, [-span, span] (span in raw units), both ends of s32, and a
 * stride across all of s32. */
static void domain_s32(visit_fn f, u16 radix, int64_t span, void *ctx)
{
    int64_t v;
    for (v = -32768; v <= 32767; v++) f((s32)v, radix, ctx);
    if (span > 32767) {
        int64_t lo = -span, hi = span;
        if (lo < INT32_MIN) lo = INT32_MIN;
        if (hi > INT32_MAX) hi = INT32_MAX;
        for (v = lo; v < -32768; v++) f((s32)v, radix, ctx);
        for (v = 32768; v <= hi; v++) f((s32)v, radix, ctx);
    }
    for (v = 0; v < 65536; v++) { f((s32)(INT32_MIN + v), radix, ctx); f((s32)(INT32_MAX - v), radix, ctx); }
    for (v = INT32_MIN; v <= INT32_MAX; v += 65521) f((s32)v, radix, ctx);
}

static double exact_in(s32 x, u16 radix) { return (double)x / ldexp(1.0, radix); }

/* x reduced mod p into [0, p), for exact references of huge angles. */
static double fmod_pos(double x, double p) { double m = fmod(x, p); return m < 0 ? m + p : m; }

/* x mod 2*pi into [0, 2*pi), accurate to ~1e-15 for |x| < 2^32: 2*pi as
 * a double is off by 2.4e-16, which fmod would multiply by x/2pi (up to
 * 8e-8 rad at 2^31, visible in tan next to a pole). Two-part 2*pi and
 * fma keep the reduction exact to double rounding. */
static double mod_2pi(double x)
{
    const double hi = 6.283185307179586232, lo = 2.4492935982947064e-16;
    double k = floor(x / hi);
    double m = fma(-k, hi, x) - k * lo;
    while (m < 0) m += hi + lo;
    while (m >= hi) m -= hi + lo;
    return m;
}

/*---------------------------------------------------------------------
 * Angle functions.
 */
typedef s32 (*fn_r)(s32, u16);

typedef struct { track_t *t; fn_r f; int deg; int kind; } angle_ctx; /* kind 0 sin, 1 cos, 2 tan */

/* Exact sin/cos/tan of x (radians or degrees) without large-argument
 * error: reduce in the input's own units first. In degrees the period is
 * exact; in radians fmod is exact, and 2*pi as a double is good to 2e-16
 * relative, i.e. < 1e-6 rad at |x| = 2^31. */
static void exact_trig(double x, int deg, double *s, double *c)
{
    if (deg) {
        double m = fmod_pos(x, 360.0);
        /* exact at multiples of 90 degrees */
        if (m == 0.0)        { *s = 0;  *c = 1;  return; }
        if (m == 90.0)       { *s = 1;  *c = 0;  return; }
        if (m == 180.0)      { *s = 0;  *c = -1; return; }
        if (m == 270.0)      { *s = -1; *c = 0;  return; }
        *s = sin(m * M_PI / 180.0); *c = cos(m * M_PI / 180.0);
    } else {
        double m = mod_2pi(x);
        *s = sin(m); *c = cos(m);
    }
}

static int is_exact_pole(s32 x, u16 radix, int deg)
{
    /* Only degrees can land exactly on a pole (radians are irrational). */
    if (!deg) return 0;
    {
        double m = fmod_pos(exact_in(x, radix), 180.0);
        return m == 90.0;
    }
}

static void tan_check(track_t *t, s32 got, double want_val, s32 in, s32 in2, int pole)
{
    if (pole) {
        tr_conv(t, got == FR_TRIG_MAXVAL || got == -FR_TRIG_MAXVAL, in, in2, got);
        return;
    }
    {
        double want = want_val * 65536.0;
        if (fabs(want) >= MAXV) {
            /* saturated, or within the error measure of the exact value
             * just past the s15.16 range (as for pow and hypot_fast8) */
            s32 s = (want > 0) ? FR_TRIG_MAXVAL : -FR_TRIG_MAXVAL;
            if (got == s) { tr_add(t, 0.0, in, in2, got, want); return; }
            tr_add(t, 100.0 * fabs((double)got - want) / fabs(want), in, in2, got, want);
            return;
        }
        {
            double den = fabs(want) > 65536.0 ? fabs(want) : 65536.0;
            tr_add(t, 100.0 * fabs((double)got - want) / den, in, in2, got, want);
        }
    }
}

static void angle_visit(s32 x, u16 radix, void *vctx)
{
    angle_ctx *c = (angle_ctx *)vctx;
    double s, co;
    s32 got = c->f(x, radix);
    exact_trig(exact_in(x, radix), c->deg, &s, &co);
    if (c->kind == 0)      tr_add(c->t, fabs((double)got - s * 65536.0), x, 0, got, s * 65536.0);
    else if (c->kind == 1) tr_add(c->t, fabs((double)got - co * 65536.0), x, 0, got, co * 65536.0);
    else tan_check(c->t, got, s / co, x, 0, is_exact_pole(x, radix, c->deg));
}

typedef struct { track_t *t; int deg; } bam_ctx;

static void tobam_visit(s32 x, u16 radix, void *vctx)
{
    bam_ctx *c = (bam_ctx *)vctx;
    double v = exact_in(x, radix);
    double turns = c->deg ? fmod_pos(v, 360.0) / 360.0 : mod_2pi(v) / (2.0 * M_PI);
    double want = turns * 65536.0;
    u16 got = c->deg ? fr_deg_to_bam(x, radix) : fr_rad_to_bam(x, radix);
    double e = fabs((double)got - want);
    if (e > 32768.0) e = 65536.0 - e;
    tr_add(c->t, e, x, 0, got, want);
}


static void test_angles(void)
{
    static const struct { const char *name; fn_r f; int deg, kind; } fns[] = {
        {"fr_sin", fr_sin, 0, 0}, {"fr_cos", fr_cos, 0, 1}, {"fr_tan", fr_tan, 0, 2},
        {"fr_sin_deg", fr_sin_deg, 1, 0}, {"fr_cos_deg", fr_cos_deg, 1, 1}, {"fr_tan_deg", fr_tan_deg, 1, 2},
    };
    int i;
    u16 r;
    track_t t;

    table_header("BAM input (every u16)");
    {
        track_t ts, tc, tt;
        u32 b;
        tr_init(&ts, "fr_sin_bam", -1, "LSB");
        tr_init(&tc, "fr_cos_bam", -1, "LSB");
        tr_init(&tt, "fr_tan_bam", -1, "%");
        for (b = 0; b < 65536u; b++) {
            double a = b * (2.0 * M_PI / 65536.0), s = sin(a), c = cos(a);
            s32 gs = fr_sin_bam((u16)b), gc = fr_cos_bam((u16)b), gt = fr_tan_bam((u16)b);
            if ((b & 0x3FFFu) == 0) { s = (b == 0x4000u) ? 1 : (b == 0xC000u) ? -1 : 0;
                                      c = (b == 0) ? 1 : (b == 0x8000u) ? -1 : 0; }
            tr_add(&ts, fabs(gs - s * 65536.0), (s32)b, 0, gs, s * 65536.0);
            tr_add(&tc, fabs(gc - c * 65536.0), (s32)b, 0, gc, c * 65536.0);
            tan_check(&tt, gt, s / c, (s32)b, 0, b == 0x4000u || b == 0xC000u);
        }
        tr_report(&ts); tr_report(&tc); tr_report(&tt);
    }

    for (i = 0; i < 6; i++) {
        char title[64];
        snprintf(title, sizeof title, "%s (every input in each domain)", fns[i].name);
        table_header(title);
        for (r = 0; r <= 16; r++) {
            angle_ctx c;
            double period = fns[i].deg ? 720.0 : 4.0 * M_PI;
            tr_init(&t, fns[i].name, r, fns[i].kind == 2 ? "%" : "LSB");
            c.t = &t; c.f = fns[i].f; c.deg = fns[i].deg; c.kind = fns[i].kind;
            domain_s32(angle_visit, r, (int64_t)(period * ldexp(1.0, r)) + 1, &c);
            tr_report(&t);
        }
    }

    for (i = 0; i < 2; i++) {
        table_header(i ? "fr_deg_to_bam" : "fr_rad_to_bam");
        for (r = 0; r <= 16; r++) {
            bam_ctx c;
            double period = i ? 720.0 : 4.0 * M_PI;
            tr_init(&t, i ? "fr_deg_to_bam" : "fr_rad_to_bam", r, "BAM");
            c.t = &t; c.deg = i;
            domain_s32(tobam_visit, r, (int64_t)(period * ldexp(1.0, r)) + 1, &c);
            tr_report(&t);
        }
    }

    /* Integer-degree forms: FR_TanI and the FR_SinI/FR_CosI macros, every
     * integer degree in the range each is defined for (TanI: all s32 via
     * the same domain; the macros: |deg| < 2^18). */
    table_header("integer degrees");
    {
        track_t ts, tc, tt;
        int64_t d;
        tr_init(&ts, "FR_SinI", -1, "LSB");
        tr_init(&tc, "FR_CosI", -1, "LSB");
        tr_init(&tt, "FR_TanI", -1, "%");
        for (d = -(1 << 18) + 1; d < (1 << 18); d++) {
            double s, c;
            s32 gs = FR_SinI((s32)d), gc = FR_CosI((s32)d);
            exact_trig((double)d, 1, &s, &c);
            tr_add(&ts, fabs(gs - s * 65536.0), (s32)d, 0, gs, s * 65536.0);
            tr_add(&tc, fabs(gc - c * 65536.0), (s32)d, 0, gc, c * 65536.0);
        }
        {
            angle_ctx c;
            c.t = &tt; c.f = fr_tan_deg; c.deg = 1; c.kind = 2;  /* fr_tan_deg(x, 0) == FR_TanI(x) */
            domain_s32(angle_visit, 0, 720, &c);
            for (d = -(1 << 18); d <= (1 << 18); d++)
                if (FR_TanI((s32)d) != fr_tan_deg((s32)d, 0)) tr_conv(&tt, 0, (s32)d, 0, FR_TanI((s32)d));
        }
        tr_report(&ts); tr_report(&tc); tr_report(&tt);
    }
}

/* --s32: every s32 input at radix 16 for the angle functions. */
static void test_angles_s32(void)
{
    static const struct { const char *name; fn_r f; int deg, kind; } fns[] = {
        {"fr_sin", fr_sin, 0, 0}, {"fr_cos", fr_cos, 0, 1}, {"fr_tan", fr_tan, 0, 2},
        {"fr_sin_deg", fr_sin_deg, 1, 0}, {"fr_cos_deg", fr_cos_deg, 1, 1}, {"fr_tan_deg", fr_tan_deg, 1, 2},
    };
    int i;
    table_header("every s32 input at radix 16");
    for (i = 0; i < 6; i++) {
        track_t t;
        angle_ctx c;
        int64_t v;
        tr_init(&t, fns[i].name, 16, fns[i].kind == 2 ? "%" : "LSB");
        c.t = &t; c.f = fns[i].f; c.deg = fns[i].deg; c.kind = fns[i].kind;
        for (v = INT32_MIN; v <= INT32_MAX; v++) angle_visit((s32)v, 16, &c);
        tr_report(&t);
        fflush(stdout);
    }
}

/*---------------------------------------------------------------------
 * Inverse trig. Output radix 16 throughout.
 */

typedef struct { track_t *ta, *tc; } ac_ctx;

/* Domain: every raw input in [-1, 1] at radix r and the rest of the
 * s32 domain (|x| > 1 must return the clamped endpoint). */
static void asincos_visit(s32 x, u16 radix, void *vctx)
{
    ac_ctx *c = (ac_ctx *)vctx;
    double v = exact_in(x, radix);
    s32 ga = FR_asin(x, radix, 16), gc = FR_acos(x, radix, 16);
    if (v > 1.0) v = 1.0;
    if (v < -1.0) v = -1.0;
    tr_add(c->ta, fabs(ga - asin(v) * 65536.0), x, 0, ga, asin(v) * 65536.0);
    tr_add(c->tc, fabs(gc - acos(v) * 65536.0), x, 0, gc, acos(v) * 65536.0);
}

static void atan_visit(s32 x, u16 radix, void *vctx)
{
    track_t *t = (track_t *)vctx;
    double want = atan(exact_in(x, radix)) * 65536.0;
    s32 got = FR_atan(x, radix, 16);
    tr_add(t, fabs(got - want), x, 0, got, want);
}

static void test_inverse(void)
{
    u16 r;
    table_header("FR_asin / FR_acos (out radix 16; |x| <= 1 every input, plus s32 domain)");
    for (r = 0; r <= 16; r++) {
        track_t ta, tc;
        ac_ctx c;
        tr_init(&ta, "FR_asin", r, "LSB");
        tr_init(&tc, "FR_acos", r, "LSB");
        c.ta = &ta; c.tc = &tc;
        domain_s32(asincos_visit, r, ((int64_t)1 << r) + 1, &c);
        tr_report(&ta); tr_report(&tc);
    }
    table_header("FR_atan (out radix 16; every input in [-64, 64], plus s32 domain)");
    for (r = 0; r <= 16; r++) {
        track_t t;
        tr_init(&t, "FR_atan", r, "LSB");
        domain_s32(atan_visit, r, (int64_t)64 << r, &t);
        tr_report(&t);
    }
    table_header("FR_atan2 (out radix 16; every (y, x) in [-512, 511]^2, plus s32 edges)");
    {
        static const s32 edge[] = { INT32_MIN, INT32_MIN + 1, -65536, -1, 0, 1, 65536, INT32_MAX - 1, INT32_MAX };
        track_t t;
        s32 y, x;
        int i, j;
        tr_init(&t, "FR_atan2", -1, "LSB");
        for (y = -512; y < 512; y++)
            for (x = -512; x < 512; x++) {
                s32 got = FR_atan2(y, x, 16);
                double want = atan2((double)y, (double)x) * 65536.0;
                tr_add(&t, fabs(got - want), y, x, got, want);
            }
        for (i = 0; i < 9; i++)
            for (j = 0; j < 9; j++) {
                s32 got = FR_atan2(edge[i], edge[j], 16);
                double want = atan2((double)edge[i], (double)edge[j]) * 65536.0;
                tr_add(&t, fabs(got - want), edge[i], edge[j], got, want);
            }
        tr_report(&t);
    }
}

/*---------------------------------------------------------------------
 * sqrt, log, pow.
 */

static void sqrt_visit(s32 x, u16 radix, void *vctx)
{
    track_t *t = (track_t *)vctx;
    s32 got = FR_sqrt(x, radix);
    if (x < 0) { tr_conv(t, got == FR_DOMAIN_ERROR, x, 0, got); return; }
    {
        double want = sqrt(exact_in(x, radix)) * ldexp(1.0, radix);
        tr_add(t, fabs(got - want), x, 0, got, want);
    }
}

typedef struct { track_t *t2, *te, *t10; } log_ctx;

static void log_visit(s32 x, u16 radix, void *vctx)
{
    log_ctx *c = (log_ctx *)vctx;
    s32 g2 = FR_log2(x, radix, 16), ge = FR_ln(x, radix, 16), g10 = FR_log10(x, radix, 16);
    if (x <= 0) {
        tr_conv(c->t2, g2 == FR_LOG2MIN, x, 0, g2);
        tr_conv(c->te, ge == FR_LOG2MIN, x, 0, ge);
        tr_conv(c->t10, g10 == FR_LOG2MIN, x, 0, g10);
        return;
    }
    {
        double v = exact_in(x, radix);
        tr_add(c->t2, fabs(g2 - log2(v) * 65536.0), x, 0, g2, log2(v) * 65536.0);
        tr_add(c->te, fabs(ge - log(v) * 65536.0), x, 0, ge, log(v) * 65536.0);
        tr_add(c->t10, fabs(g10 - log10(v) * 65536.0), x, 0, g10, log10(v) * 65536.0);
    }
}

static void pow_check(track_t *t, s32 got, double want_val, s32 x, u16 radix)
{
    double want = want_val * ldexp(1.0, radix);
    if (want >= MAXV + 0.5) {
        /* must saturate, or (approximate forms) be within the error
         * measure of the exact value just past the s32 range */
        if (got == FR_OVERFLOW_POS) { tr_add(t, 0.0, x, 0, got, want); return; }
        tr_add(t, 100.0 * fabs(got - want) / want, x, 0, got, want);
        return;
    }
    {
        /* relative to max(|exact|, 2^16 LSBs): plain relative error once
         * the output has 16 significant bits, and below that the error in
         * units of 2^16 LSBs, so output rounding at a low radix (2^-1 at
         * radix 0 -> 0) is not counted as a 50% error. */
        double den = want > 65536.0 ? want : 65536.0;
        tr_add(t, 100.0 * fabs(got - want) / den, x, 0, got, want);
    }
}

typedef struct { track_t *t2, *te, *t10, *tef, *t10f; } pow_ctx;

static void pow_visit(s32 x, u16 radix, void *vctx)
{
    pow_ctx *c = (pow_ctx *)vctx;
    double v = exact_in(x, radix);
    pow_check(c->t2, FR_pow2(x, radix), pow(2.0, v), x, radix);
    pow_check(c->te, FR_EXP(x, radix), exp(v), x, radix);
    pow_check(c->t10, FR_POW10(x, radix), pow(10.0, v), x, radix);
    pow_check(c->tef, FR_EXP_FAST(x, radix), exp(v), x, radix);
    pow_check(c->t10f, FR_POW10_FAST(x, radix), pow(10.0, v), x, radix);
}

static void test_sqrt_log_pow(void)
{
    u16 r;
    table_header("FR_sqrt (every input in [0, 2^24), plus s32 domain)");
    for (r = 0; r <= 16; r++) {
        track_t t;
        int64_t v;
        tr_init(&t, "FR_sqrt", r, "LSB");
        for (v = 32768; v < (1 << 24); v++) sqrt_visit((s32)v, r, &t);
        domain_s32(sqrt_visit, r, 0, &t);
        tr_report(&t);
    }
    table_header("FR_log2 / FR_ln / FR_log10 (out radix 16; every input in [1, 2^24), plus s32 domain)");
    for (r = 0; r <= 16; r++) {
        track_t t2, te, t10;
        log_ctx c;
        int64_t v;
        tr_init(&t2, "FR_log2", r, "LSB");
        tr_init(&te, "FR_ln", r, "LSB");
        tr_init(&t10, "FR_log10", r, "LSB");
        c.t2 = &t2; c.te = &te; c.t10 = &t10;
        for (v = 32768; v < (1 << 24); v++) log_visit((s32)v, r, &c);
        domain_s32(log_visit, r, 0, &c);
        tr_report(&t2); tr_report(&te); tr_report(&t10);
    }
    table_header("FR_pow2 / FR_EXP / FR_POW10 (+_FAST) (every input in [-32, 32], plus s32 domain)");
    for (r = 0; r <= 16; r++) {
        track_t t2, te, t10, tef, t10f;
        pow_ctx c;
        tr_init(&t2, "FR_pow2", r, "%");
        tr_init(&te, "FR_EXP", r, "%");
        tr_init(&t10, "FR_POW10", r, "%");
        tr_init(&tef, "FR_EXP_FAST", r, "%");
        tr_init(&t10f, "FR_POW10_FAST", r, "%");
        c.t2 = &t2; c.te = &te; c.t10 = &t10; c.tef = &tef; c.t10f = &t10f;
        domain_s32(pow_visit, r, (int64_t)32 << r, &c);
        tr_report(&t2); tr_report(&te); tr_report(&t10); tr_report(&tef); tr_report(&t10f);
    }
}

/*---------------------------------------------------------------------
 * hypot. Scale-invariant (the radix argument of FR_hypot does not change
 * the arithmetic), so every (x, y) in [-512, 511]^2, every x in s16 with
 * y in a set of edges, and all pairs of s32 edges. Where the exact value
 * is >= 2^31 the result must be FR_OVERFLOW_POS.
 */

static void hyp_one(track_t *te, track_t *t8, s32 x, s32 y)
{
    double want = sqrt((double)x * x + (double)y * y);
    s32 g = FR_hypot(x, y, 16), g8 = FR_hypot_fast8(x, y);
    if (want >= MAXV + 0.5) {
        tr_conv(te, g == FR_OVERFLOW_POS, x, y, g);
        /* the approximation may land just under 2^31 */
        tr_add(t8, g8 == FR_OVERFLOW_POS ? 0.0 : 100.0 * fabs(g8 - want) / want, x, y, g8, want);
        return;
    }
    tr_add(te, fabs(g - want), x, y, g, want);
    tr_add(t8, 100.0 * fabs(g8 - want) / (want > 65536.0 ? want : 65536.0), x, y, g8, want);
}

static void test_hypot(void)
{
    static const s32 edge[] = { INT32_MIN, INT32_MIN + 1, -1518500250, -1073741824, -65536, -1, 0,
                                1, 65536, 1073741824, 1518500249, 1518500250, INT32_MAX - 1, INT32_MAX };
    const int ne = (int)(sizeof edge / sizeof edge[0]);
    track_t te, t8;
    s32 x, y;
    int i, j;
    table_header("FR_hypot / FR_hypot_fast8");
    tr_init(&te, "FR_hypot", -1, "LSB");
    tr_init(&t8, "FR_hypot_fast8", -1, "%");
    for (y = -512; y < 512; y++)
        for (x = -512; x < 512; x++) hyp_one(&te, &t8, x, y);
    for (x = -32768; x <= 32767; x++)
        for (j = 0; j < ne; j++) { hyp_one(&te, &t8, x, edge[j]); hyp_one(&te, &t8, edge[j], x); }
    for (i = 0; i < ne; i++)
        for (j = 0; j < ne; j++) hyp_one(&te, &t8, edge[i], edge[j]);
    tr_report(&te); tr_report(&t8);
}

int main(int argc, char **argv)
{
    int i, s32mode = 0;
    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--s32")) s32mode = 1;
    }
    printf("# FR_Math exhaustive accuracy tests\n");
    test_angles();
    test_inverse();
    test_sqrt_log_pow();
    test_hypot();
    if (s32mode) test_angles_s32();
    printf("\n%s\n", g_fail ? "EXHAUSTIVE TESTS: FAIL" : "EXHAUSTIVE TESTS: PASS");
    return g_fail;
}
