/**
 * test_pole_table.c — dump values around both tan poles (90° and 270°)
 *
 * For ±20 entries around each pole, show:
 *   BAM index, degrees, ground truth, and each function's output + error
 */

#include <stdio.h>
#include <math.h>
#include "FR_math.h"
#include "FR_trig_table.h"
#include "FR_tan_table.h"

extern s32 fr_tan_bam32(u16 bam);
extern s32 fr_tan32(s32 rad, u16 radix);
extern s32 fr_tan_deg32(s32 deg, u16 radix);

static double to_dbl(s32 v) { return (double)v / 65536.0; }

static double err_pct(double ref, double got)
{
    double ae = fabs(got - ref);
    if (fabs(ref) >= 0.01)
        return (ae / fabs(ref)) * 100.0;
    return ae * 100.0;  /* absolute near zero */
}

static void dump_pole(u16 pole_bam, const char *name, int range)
{
    printf("\n### Pole at %s (BAM %u)\n\n", name, pole_bam);
    printf("| %5s | %9s | %14s | %14s %7s | %14s %7s | %14s %7s | %14s %7s |\n",
           "BAM", "deg", "ground truth",
           "tan_bam OLD", "err%",
           "tan_bam32 NEW", "err%",
           "tan(rad) NEW", "err%",
           "tan(deg) NEW", "err%");
    printf("| %5s | %9s | %14s | %14s %7s | %14s %7s | %14s %7s | %14s %7s |\n",
           "-----", "---------", "--------------",
           "--------------", "-------",
           "--------------", "-------",
           "--------------", "-------",
           "--------------", "-------");

    for (int i = -range; i <= range; i++) {
        u16 bam = (u16)((int)pole_bam + i);
        double rad_dbl = (double)bam * 2.0 * M_PI / 65536.0;
        double deg_dbl = (double)bam * 360.0 / 65536.0;
        double truth   = tan(rad_dbl);

        /* BAM functions */
        double v_bam_old = to_dbl(fr_tan_bam(bam));
        double v_bam_new = to_dbl(fr_tan_bam32(bam));

        /* Radian: convert BAM to s15.16 radian the same way the library does */
        s32 r16 = (s32)(rad_dbl * 65536.0 + (rad_dbl >= 0 ? 0.5 : -0.5));
        double v_rad_new = to_dbl(fr_tan32(r16, 16));

        /* Degree: convert to s9.6 */
        s16 d6 = (s16)(int)(deg_dbl * 64.0 + (deg_dbl >= 0 ? 0.5 : -0.5));
        double v_deg_new = to_dbl(fr_tan_deg32(d6, 6));

        /* Clip display for readability */
        if (fabs(truth) > 100000.0) {
            printf("| %5u | %9.3f | %14s | %14s %7s | %14s %7s | %14s %7s | %14s %7s |\n",
                   bam, deg_dbl, ">>pole<<",
                   "---", "---", "---", "---", "---", "---", "---", "---");
            continue;
        }

        printf("| %5u | %9.3f | %14.4f | %14.4f %6.2f%% | %14.4f %6.2f%% | %14.4f %6.2f%% | %14.4f %6.2f%% |\n",
               bam, deg_dbl, truth,
               v_bam_old, err_pct(truth, v_bam_old),
               v_bam_new, err_pct(truth, v_bam_new),
               v_rad_new, err_pct(truth, v_rad_new),
               v_deg_new, err_pct(truth, v_deg_new));
    }
}

int main(void)
{
    printf("FR_Math tan pole neighborhood dump\n");
    printf("==================================\n");
    printf("Values within ±20 BAM steps of each pole.\n");
    printf("Error: relative %% when |ref|>=0.01, absolute otherwise.\n");

    /* 90° pole = BAM 16384, 270° pole = BAM 49152 */
    dump_pole(16384, "90 deg", 20);
    dump_pole(49152, "270 deg", 20);

    printf("\nDone.\n");
    return 0;
}
