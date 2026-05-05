/**
 * trig_accuracy.cpp — FR_math vs libfixmath vs IEEE double trig comparison
 *
 * Sweeps 0-360 degrees in 1-degree steps for sin, cos, tan.
 * Prints a per-degree detail table and a summary table.
 *
 * Requires libfixmath source at compare_lfm/libfixmath/libfixmath/.
 * Build:  make ex_trig_accuracy   (only built if libfixmath is present)
 * Run:    ./build/ex_trig_accuracy
 *
 * Copyright (C) 2001-2026 M. A. Chatterjee — zlib license (see FR_math.h)
 */

#include <stdio.h>
#include <cmath>

#include "FR_defs.h"
#include "FR_math.h"
#include "fixmath.h"

static double pct_err(double measured, double ref)
{
    if (fabs(ref) < 1e-12)
        return fabs(measured) * 100.0;
    return ((measured - ref) / ref) * 100.0;
}

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

/* ================================================================== */
int main()
{
    printf("FR_Math vs libfixmath — Trig Accuracy Comparison  (v%s)\n\n", FR_MATH_VERSION);

    err_stats_t fr_sin_e, fr_cos_e, fr_tan_e;
    err_stats_t lf_sin_e, lf_cos_e, lf_tan_e;
    err_reset(&fr_sin_e); err_reset(&fr_cos_e); err_reset(&fr_tan_e);
    err_reset(&lf_sin_e); err_reset(&lf_cos_e); err_reset(&lf_tan_e);

    /* Header */
    printf("  deg | FR_sin    LFM_sin   ref_sin    FR_err%%  LFM_err%%"
           " | FR_cos    LFM_cos   ref_cos    FR_err%%  LFM_err%%"
           " | FR_tan      LFM_tan     ref_tan      FR_err%%   LFM_err%%\n");
    printf("  ----+");
    for (int g = 0; g < 3; g++) printf("--------------------------------------------------------------+");
    printf("\n");

    for (int deg = 0; deg <= 360; deg++) {
        double rad_d = deg * M_PI / 180.0;

        /* IEEE double reference */
        double ref_s = sin(rad_d);
        double ref_c = cos(rad_d);
        double ref_t = tan(rad_d);

        /* FR_math: integer-degree API, result is s15.16 */
        double fr_s = (double)FR_SinI(deg) / 65536.0;
        double fr_c = (double)FR_CosI(deg) / 65536.0;
        double fr_t = (double)FR_TanI(deg) / 65536.0;

        /* libfixmath: convert degrees to radians as fix16_t */
        fix16_t lf_rad = fix16_from_dbl(rad_d);
        double lf_s = fix16_to_dbl(fix16_sin(lf_rad));
        double lf_c = fix16_to_dbl(fix16_cos(lf_rad));
        double lf_t = fix16_to_dbl(fix16_tan(lf_rad));

        /* Error calculation */
        double fse = pct_err(fr_s, ref_s);
        double fce = pct_err(fr_c, ref_c);
        double fte = pct_err(fr_t, ref_t);
        double lse = pct_err(lf_s, ref_s);
        double lce = pct_err(lf_c, ref_c);
        double lte = pct_err(lf_t, ref_t);

        err_add(&fr_sin_e, fse); err_add(&fr_cos_e, fce); err_add(&fr_tan_e, fte);
        err_add(&lf_sin_e, lse); err_add(&lf_cos_e, lce); err_add(&lf_tan_e, lte);

        /* Clamp tan display for readability near poles */
        int tan_pole = (fabs(ref_t) > 1000.0) ? 1 : 0;

        printf("  %3d |", deg);
        printf(" %8.5f %8.5f %8.5f %8.3f%% %8.3f%%",
               fr_s, lf_s, ref_s, fse, lse);
        printf(" |");
        printf(" %8.5f %8.5f %8.5f %8.3f%% %8.3f%%",
               fr_c, lf_c, ref_c, fce, lce);
        printf(" |");
        if (tan_pole)
            printf(" %10.1f  %10.1f  %10.1f  (pole)    (pole)",
                   fr_t, lf_t, ref_t);
        else
            printf(" %10.5f %10.5f %10.5f %9.3f%% %9.3f%%",
                   fr_t, lf_t, ref_t, fte, lte);
        printf("\n");
    }

    /* Summary table */
    printf("\n  ============================================================\n");
    printf("  Summary\n");
    printf("  ============================================================\n\n");
    printf("  %-10s | %12s %12s | %12s %12s\n",
           "function", "FR_max%", "FR_avg%", "LFM_max%", "LFM_avg%");
    printf("  %-10s-+-%-12s-%-12s-+-%-12s-%-12s\n",
           "----------", "------------", "------------",
           "------------", "------------");

    #define SUMMARY_ROW(name, fr_e, lf_e)                                 \
        printf("  %-10s | %11.4f%% %11.4f%% | %11.4f%% %11.4f%%\n",      \
               name,                                                       \
               (fr_e).max_abs_pct,                                         \
               (fr_e).n > 0 ? (fr_e).sum_abs_pct / (fr_e).n : 0.0,        \
               (lf_e).max_abs_pct,                                         \
               (lf_e).n > 0 ? (lf_e).sum_abs_pct / (lf_e).n : 0.0)

    SUMMARY_ROW("sin", fr_sin_e, lf_sin_e);
    SUMMARY_ROW("cos", fr_cos_e, lf_cos_e);
    SUMMARY_ROW("tan", fr_tan_e, lf_tan_e);

    printf("\n  FR_math:    FR_SinI/FR_CosI/FR_TanI  (integer degrees, s15.16 output)\n");
    printf("  libfixmath: fix16_sin/cos/tan         (fix16_t radians, Q16.16 output)\n");
    printf("  Reference:  IEEE 754 double sin/cos/tan\n");

    printf("\n--- end ---\n");
    return 0;
}
