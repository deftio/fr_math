/**
 * log_exp_curves.cpp — Sweep log/exp/sqrt functions with comparison tables
 *
 * For each function group, prints a table of FR_math result vs IEEE double
 * reference with error%, then a summary line (max_err%, avg_err%).
 *
 * Build:  make ex_logexp
 * Run:    ./build/ex_logexp
 *
 * Copyright (C) 2001-2026 M. A. Chatterjee — zlib license (see FR_math.h)
 */

#include <stdio.h>
#include <math.h>

#include "FR_defs.h"
#include "FR_math.h"

#define R 16  /* working radix for all tests */

static double pct_err(double measured, double ref)
{
    if (fabs(ref) < 1e-12)
        return fabs(measured) * 100.0;
    return ((measured - ref) / ref) * 100.0;
}

/* Accumulator for max/avg error tracking */
typedef struct {
    double max_abs_pct;
    double sum_abs_pct;
    int    n;
} err_stats_t;

static void err_reset(err_stats_t *e) { e->max_abs_pct = 0; e->sum_abs_pct = 0; e->n = 0; }

static void err_add(err_stats_t *e, double pct)
{
    double a = fabs(pct);
    if (a > e->max_abs_pct) e->max_abs_pct = a;
    e->sum_abs_pct += a;
    e->n++;
}

static void err_print(err_stats_t *e, const char *label)
{
    printf("  %s  max |err|: %.4f%%   avg |err|: %.4f%%   (%d points)\n",
           label, e->max_abs_pct,
           e->n > 0 ? e->sum_abs_pct / e->n : 0.0, e->n);
}

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
    printf("FR_Math — Log / Exp / Sqrt Curves  (v%s, radix=%d)\n", FR_MATH_VERSION, R);

    /* -------------------------------------------------------------- */
    /* Log functions: FR_log2, FR_ln, FR_log10                        */
    /* -------------------------------------------------------------- */
    section("Log functions (input > 0)");

    double log_inputs[] = {0.25, 0.5, 1.0, 1.5, 2.0, 3.0, 5.0, 7.0, 10.0};
    int n_log = (int)(sizeof(log_inputs) / sizeof(log_inputs[0]));

    err_stats_t e_log2, e_ln, e_log10;
    err_reset(&e_log2); err_reset(&e_ln); err_reset(&e_log10);

    printf("  %-8s | %-12s %-12s %-8s | %-12s %-12s %-8s | %-12s %-12s %-8s\n",
           "input", "FR_log2", "ref_log2", "err%",
           "FR_ln", "ref_ln", "err%",
           "FR_log10", "ref_log10", "err%");
    printf("  %-8s-+-%-12s-%-12s-%-8s-+-%-12s-%-12s-%-8s-+-%-12s-%-12s-%-8s\n",
           "--------", "------------", "------------", "--------",
           "------------", "------------", "--------",
           "------------", "------------", "--------");

    for (int i = 0; i < n_log; i++) {
        double x = log_inputs[i];
        s32 xfr = (s32)(x * (1 << R));

        double fr_l2  = FR2D(FR_log2(xfr, R, R), R);
        double fr_ln  = FR2D(FR_ln(xfr, R, R), R);
        double fr_l10 = FR2D(FR_log10(xfr, R, R), R);

        double ref_l2  = log2(x);
        double ref_ln  = log(x);
        double ref_l10 = log10(x);

        double e2 = pct_err(fr_l2, ref_l2);
        double en = pct_err(fr_ln, ref_ln);
        double e10 = pct_err(fr_l10, ref_l10);

        err_add(&e_log2, e2);
        err_add(&e_ln, en);
        err_add(&e_log10, e10);

        printf("  %-8.4f | %12.6f %12.6f %7.3f%% | %12.6f %12.6f %7.3f%% | %12.6f %12.6f %7.3f%%\n",
               x, fr_l2, ref_l2, e2, fr_ln, ref_ln, en, fr_l10, ref_l10, e10);
    }

    printf("\n");
    err_print(&e_log2,  "FR_log2 ");
    err_print(&e_ln,    "FR_ln   ");
    err_print(&e_log10, "FR_log10");

    /* -------------------------------------------------------------- */
    /* Exp functions: FR_pow2, FR_EXP, FR_POW10                       */
    /* -------------------------------------------------------------- */
    section("Exp functions (input -3.0 to 3.0)");

    err_stats_t e_pow2, e_exp, e_pow10;
    err_reset(&e_pow2); err_reset(&e_exp); err_reset(&e_pow10);

    printf("  %-8s | %-12s %-12s %-8s | %-12s %-12s %-8s | %-12s %-12s %-8s\n",
           "input", "FR_pow2", "ref_pow2", "err%",
           "FR_EXP", "ref_exp", "err%",
           "FR_POW10", "ref_pow10", "err%");
    printf("  %-8s-+-%-12s-%-12s-%-8s-+-%-12s-%-12s-%-8s-+-%-12s-%-12s-%-8s\n",
           "--------", "------------", "------------", "--------",
           "------------", "------------", "--------",
           "------------", "------------", "--------");

    for (double x = -3.0; x <= 3.001; x += 0.5) {
        s32 xfr = (s32)(x * (1 << R));

        double fr_p2  = FR2D(FR_pow2(xfr, R), R);
        double fr_ex  = FR2D(FR_EXP(xfr, R), R);
        double fr_p10 = FR2D(FR_POW10(xfr, R), R);

        double ref_p2  = pow(2.0, x);
        double ref_ex  = exp(x);
        double ref_p10 = pow(10.0, x);

        double ep2 = pct_err(fr_p2, ref_p2);
        double eex = pct_err(fr_ex, ref_ex);
        double ep10 = pct_err(fr_p10, ref_p10);

        err_add(&e_pow2, ep2);
        err_add(&e_exp, eex);
        err_add(&e_pow10, ep10);

        printf("  %-8.2f | %12.6f %12.6f %7.3f%% | %12.6f %12.6f %7.3f%% | %12.6f %12.6f %7.3f%%\n",
               x, fr_p2, ref_p2, ep2, fr_ex, ref_ex, eex, fr_p10, ref_p10, ep10);
    }

    printf("\n");
    err_print(&e_pow2,  "FR_pow2 ");
    err_print(&e_exp,   "FR_EXP  ");
    err_print(&e_pow10, "FR_POW10");

    /* -------------------------------------------------------------- */
    /* Square root: FR_sqrt                                           */
    /* -------------------------------------------------------------- */
    section("Square root (FR_sqrt)");

    double sqrt_inputs[] = {0.25, 0.5, 1.0, 2.0, 3.0, 4.0, 5.0, 7.0,
                            9.0, 10.0, 16.0, 25.0, 50.0, 64.0, 100.0};
    int n_sqrt = (int)(sizeof(sqrt_inputs) / sizeof(sqrt_inputs[0]));

    err_stats_t e_sqrt;
    err_reset(&e_sqrt);

    printf("  %-10s | %-14s %-14s %-8s\n",
           "input", "FR_sqrt", "ref_sqrt", "err%");
    printf("  %-10s-+-%-14s-%-14s-%-8s\n",
           "----------", "--------------", "--------------", "--------");

    for (int i = 0; i < n_sqrt; i++) {
        double x = sqrt_inputs[i];
        s32 xfr = (s32)(x * (1 << R));
        double fr_sq = FR2D(FR_sqrt(xfr, R), R);
        double ref_sq = sqrt(x);
        double esq = pct_err(fr_sq, ref_sq);
        err_add(&e_sqrt, esq);
        printf("  %-10.4f | %14.6f %14.6f %7.3f%%\n",
               x, fr_sq, ref_sq, esq);
    }

    printf("\n");
    err_print(&e_sqrt, "FR_sqrt ");

    printf("\n--- end ---\n");
    return 0;
}
