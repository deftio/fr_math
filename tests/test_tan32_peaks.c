/*
 * test_tan32_peaks.c - Find peak error locations and print ±20 entries around them
 */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "../src/FR_math.h"

extern s32 fr_tan_bam32(u16 bam);
extern s32 fr_tan32(s32 rad, u16 radix);
extern s32 fr_tan_deg32(s32 deg, u16 radix);

static double fr2d(s32 val, int radix) {
    return (double)val / (double)(1L << radix);
}

static double tan_err(double val, double ref) {
    if (fabs(ref) > 0.001)
        return fabs((val - ref) / ref) * 100.0;
    else
        return fabs(val - ref) * 100.0;
}

/*=======================================================
 * BAM peak finder + neighborhood dump
 */
static void peak_tan_bam(void)
{
    s32 bam;
    s32 old_peak_bam = 0, new_peak_bam = 0;
    double old_peak = 0, new_peak = 0;

    /* Pass 1: find peaks */
    for (bam = 0; bam < 65536; bam++) {
        double angle = (double)bam * 2.0 * M_PI / 65536.0;
        double ref = tan(angle);
        if (fabs(ref) > 500.0) continue;
        double ov = fr2d(fr_tan_bam((u16)bam), 16);
        double nv = fr2d(fr_tan_bam32((u16)bam), 16);
        double oe = tan_err(ov, ref);
        double ne = tan_err(nv, ref);
        if (oe > old_peak) { old_peak = oe; old_peak_bam = bam; }
        if (ne > new_peak) { new_peak = ne; new_peak_bam = bam; }
    }

    printf("## tan BAM: OLD peak at BAM %d (%.4f deg), NEW peak at BAM %d (%.4f deg)\n\n",
        (int)old_peak_bam, old_peak_bam * 360.0 / 65536.0,
        (int)new_peak_bam, new_peak_bam * 360.0 / 65536.0);

    /* Pass 2: dump ±20 around OLD peak */
    printf("### OLD peak neighborhood (BAM %d ± 20)\n\n", (int)old_peak_bam);
    printf("| BAM   | deg       | ref (libm)     | OLD result     | OLD err %%   | NEW result     | NEW err %%   |\n");
    printf("|-------|-----------|----------------|----------------|-------------|----------------|-------------|\n");
    for (bam = old_peak_bam - 20; bam <= old_peak_bam + 20; bam++) {
        u16 b = (u16)(bam & 0xFFFF);
        double angle = (double)b * 2.0 * M_PI / 65536.0;
        double ref = tan(angle);
        if (fabs(ref) > 500.0) { printf("| %5d | %9.4f | (pole)         |                |             |                |             |\n", bam, b * 360.0 / 65536.0); continue; }
        double ov = fr2d(fr_tan_bam(b), 16);
        double nv = fr2d(fr_tan_bam32(b), 16);
        printf("| %5d | %9.4f | %14.8f | %14.8f | %11.6f | %14.8f | %11.6f |%s\n",
            bam, b * 360.0 / 65536.0, ref, ov, tan_err(ov, ref), nv, tan_err(nv, ref),
            (bam == old_peak_bam) ? " <-- OLD PEAK" : (bam == new_peak_bam) ? " <-- NEW PEAK" : "");
    }

    if (abs((int)(new_peak_bam - old_peak_bam)) > 25) {
        printf("\n### NEW peak neighborhood (BAM %d ± 20)\n\n", (int)new_peak_bam);
        printf("| BAM   | deg       | ref (libm)     | OLD result     | OLD err %%   | NEW result     | NEW err %%   |\n");
        printf("|-------|-----------|----------------|----------------|-------------|----------------|-------------|\n");
        for (bam = new_peak_bam - 20; bam <= new_peak_bam + 20; bam++) {
            u16 b = (u16)(bam & 0xFFFF);
            double angle = (double)b * 2.0 * M_PI / 65536.0;
            double ref = tan(angle);
            if (fabs(ref) > 500.0) { printf("| %5d | %9.4f | (pole)         |                |             |                |             |\n", bam, b * 360.0 / 65536.0); continue; }
            double ov = fr2d(fr_tan_bam(b), 16);
            double nv = fr2d(fr_tan_bam32(b), 16);
            printf("| %5d | %9.4f | %14.8f | %14.8f | %11.6f | %14.8f | %11.6f |%s\n",
                bam, b * 360.0 / 65536.0, ref, ov, tan_err(ov, ref), nv, tan_err(nv, ref),
                (bam == new_peak_bam) ? " <-- NEW PEAK" : "");
        }
    }

    printf("\n");
}

/*=======================================================
 * Radian peak finder + neighborhood dump
 */
static void peak_tan_rad(void)
{
    s32 rad16;
    s32 old_peak_r = 0, new_peak_r = 0;
    double old_peak = 0, new_peak = 0;

    for (rad16 = -65536; rad16 <= 65535; rad16++) {
        double angle = (double)rad16 / 65536.0;
        double ref = tan(angle);
        if (fabs(ref) > 500.0) continue;
        double ov = fr2d(fr_tan(rad16, 16), 16);
        double nv = fr2d(fr_tan32(rad16, 16), 16);
        double oe = tan_err(ov, ref);
        double ne = tan_err(nv, ref);
        if (oe > old_peak) { old_peak = oe; old_peak_r = rad16; }
        if (ne > new_peak) { new_peak = ne; new_peak_r = rad16; }
    }

    printf("## tan Radian: OLD peak at r16=%d (%.6f rad, %.4f deg), NEW peak at r16=%d (%.6f rad, %.4f deg)\n\n",
        (int)old_peak_r, old_peak_r / 65536.0, old_peak_r / 65536.0 * 180.0 / M_PI,
        (int)new_peak_r, new_peak_r / 65536.0, new_peak_r / 65536.0 * 180.0 / M_PI);

    /* dump around OLD peak */
    printf("### OLD peak neighborhood (r16=%d ± 20)\n\n", (int)old_peak_r);
    printf("| r16    | rad         | deg       | ref (libm)     | OLD result     | OLD err %%   | NEW result     | NEW err %%   |\n");
    printf("|--------|-------------|-----------|----------------|----------------|-------------|----------------|-------------|\n");
    for (rad16 = old_peak_r - 20; rad16 <= old_peak_r + 20; rad16++) {
        double angle = (double)rad16 / 65536.0;
        double ref = tan(angle);
        if (fabs(ref) > 500.0) continue;
        double ov = fr2d(fr_tan(rad16, 16), 16);
        double nv = fr2d(fr_tan32(rad16, 16), 16);
        printf("| %6d | %11.7f | %9.4f | %14.8f | %14.8f | %11.6f | %14.8f | %11.6f |%s\n",
            (int)rad16, angle, angle * 180.0 / M_PI, ref, ov, tan_err(ov, ref), nv, tan_err(nv, ref),
            (rad16 == old_peak_r) ? " <-- OLD PEAK" : (rad16 == new_peak_r) ? " <-- NEW PEAK" : "");
    }

    if (abs((int)(new_peak_r - old_peak_r)) > 25) {
        printf("\n### NEW peak neighborhood (r16=%d ± 20)\n\n", (int)new_peak_r);
        printf("| r16    | rad         | deg       | ref (libm)     | OLD result     | OLD err %%   | NEW result     | NEW err %%   |\n");
        printf("|--------|-------------|-----------|----------------|----------------|-------------|----------------|-------------|\n");
        for (rad16 = new_peak_r - 20; rad16 <= new_peak_r + 20; rad16++) {
            double angle = (double)rad16 / 65536.0;
            double ref = tan(angle);
            if (fabs(ref) > 500.0) continue;
            double ov = fr2d(fr_tan(rad16, 16), 16);
            double nv = fr2d(fr_tan32(rad16, 16), 16);
            printf("| %6d | %11.7f | %9.4f | %14.8f | %14.8f | %11.6f | %14.8f | %11.6f |%s\n",
                (int)rad16, angle, angle * 180.0 / M_PI, ref, ov, tan_err(ov, ref), nv, tan_err(nv, ref),
                (rad16 == new_peak_r) ? " <-- NEW PEAK" : "");
        }
    }

    printf("\n");
}

/*=======================================================
 * Degree peak finder + neighborhood dump
 */
static void peak_tan_deg(void)
{
    s16 deg;
    s16 old_peak_d = 0, new_peak_d = 0;
    double old_peak = 0, new_peak = 0;

    for (deg = -180; deg <= 179; deg++) {
        double ref = tan((double)deg * M_PI / 180.0);
        if (fabs(ref) > 500.0) continue;
        double ov = fr2d(FR_TanI(deg), 16);
        double nv = fr2d(fr_tan_deg32(deg, 0), 16);
        double oe = tan_err(ov, ref);
        double ne = tan_err(nv, ref);
        if (oe > old_peak) { old_peak = oe; old_peak_d = deg; }
        if (ne > new_peak) { new_peak = ne; new_peak_d = deg; }
    }

    printf("## tan Degree: OLD peak at %d deg, NEW peak at %d deg\n\n",
        (int)old_peak_d, (int)new_peak_d);

    /* dump full range around both peaks, ±20 deg */
    s16 lo = old_peak_d < new_peak_d ? old_peak_d : new_peak_d;
    s16 hi = old_peak_d > new_peak_d ? old_peak_d : new_peak_d;
    lo = (lo - 20 < -180) ? -180 : lo - 20;
    hi = (hi + 20 > 179) ? 179 : hi + 20;

    printf("### Neighborhood (%d .. %d deg)\n\n", (int)lo, (int)hi);
    printf("| deg  | ref (libm)     | OLD result     | OLD err %%   | NEW result     | NEW err %%   |\n");
    printf("|------|----------------|----------------|-------------|----------------|-------------|\n");
    for (deg = lo; deg <= hi; deg++) {
        double ref = tan((double)deg * M_PI / 180.0);
        if (fabs(ref) > 500.0) { printf("| %4d | (pole)         |                |             |                |             |\n", (int)deg); continue; }
        double ov = fr2d(FR_TanI(deg), 16);
        double nv = fr2d(fr_tan_deg32(deg, 0), 16);
        printf("| %4d | %14.8f | %14.8f | %11.6f | %14.8f | %11.6f |%s\n",
            (int)deg, ref, ov, tan_err(ov, ref), nv, tan_err(nv, ref),
            (deg == old_peak_d && deg == new_peak_d) ? " <-- BOTH PEAK" :
            (deg == old_peak_d) ? " <-- OLD PEAK" :
            (deg == new_peak_d) ? " <-- NEW PEAK" : "");
    }
    printf("\n");
}

int main(void)
{
    printf("# Peak Error Neighborhoods for Tangent Functions\n\n");
    peak_tan_bam();
    peak_tan_rad();
    peak_tan_deg();
    return 0;
}
