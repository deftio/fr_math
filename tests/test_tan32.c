/*
 * test_tan32.c - Head-to-head comparison of LUT32 tan/atan2 vs current impls
 *
 * Compares:
 *   fr_tan_bam32()  vs  fr_tan_bam()   — BAM accuracy + speed
 *   fr_tan32()      vs  fr_tan()        — radian accuracy
 *   fr_tan_deg32()  vs  FR_TanI()       — integer-degree accuracy
 *   fr_atan2_32()   vs  FR_atan2()      — accuracy + speed
 *
 * Compile:
 *   cc -Isrc -Wall -Os src/FR_tan32.c src/FR_math.c tests/test_tan32.c -lm -o build/test_tan32
 *
 * @author M A Chatterjee <deftio [at] deftio [dot] com>
 */

#include <stdio.h>
#include <math.h>
#include <time.h>
#include <stdlib.h>
#include "../src/FR_math.h"

/* Declarations for the new LUT32 functions (in FR_tan32.c) */
extern s32 fr_tan_bam32(u16 bam);
extern s32 fr_tan32(s32 rad, u16 radix);
extern s32 fr_tan_deg32(s32 deg, u16 radix);
extern s32 fr_atan2_32(s32 y, s32 x, u16 out_radix);

/*=======================================================
 * Helpers
 */
static double fr2d(s32 val, int radix) {
    return (double)val / (double)(1L << radix);
}

/*=======================================================
 * Test 1: Tangent accuracy sweep — all 65536 BAM points
 */
static void test_tan_bam_accuracy(void)
{
    double max_err_old = 0.0, max_err_new = 0.0;
    double sum_err_old = 0.0, sum_err_new = 0.0;
    u16 max_bam_old = 0, max_bam_new = 0;
    int count = 0;
    u16 bam;

    printf("## Tangent BAM Accuracy (65536 BAM points)\n\n");

    for (bam = 0; bam < 0xFFFFu; bam++) {
        double angle = (double)bam * 2.0 * M_PI / 65536.0;
        double ref = tan(angle);
        double old_val, new_val, err_old, err_new;

        /* Skip near poles where tan -> infinity (within ~1 deg of 90/270) */
        if (fabs(ref) > 500.0) continue;

        old_val = fr2d(fr_tan_bam(bam), 16);
        new_val = fr2d(fr_tan_bam32(bam), 16);

        /* Percentage error relative to reference */
        if (fabs(ref) > 0.001) {
            err_old = fabs((old_val - ref) / ref) * 100.0;
            err_new = fabs((new_val - ref) / ref) * 100.0;
        } else {
            /* Near zero, use absolute error scaled to % of 1.0 */
            err_old = fabs(old_val - ref) * 100.0;
            err_new = fabs(new_val - ref) * 100.0;
        }

        sum_err_old += err_old;
        sum_err_new += err_new;
        if (err_old > max_err_old) { max_err_old = err_old; max_bam_old = bam; }
        if (err_new > max_err_new) { max_err_new = err_new; max_bam_new = bam; }
        count++;
    }

    printf("| Metric         | Current (fr_tan_bam) | LUT32 (fr_tan_bam32) |\n");
    printf("|----------------|----------------------|----------------------|\n");
    printf("| Peak error (%%) |          %11.6f |          %11.6f |\n", max_err_old, max_err_new);
    printf("| Avg error (%%)  |          %11.6f |          %11.6f |\n", sum_err_old / count, sum_err_new / count);
    printf("| Peak BAM       |              0x%04X  |              0x%04X  |\n", max_bam_old, max_bam_new);
    printf("| Points tested  |              %6d  |              %6d  |\n", count, count);
    printf("\n");
}

/*=======================================================
 * Test 2: Tangent radian accuracy — sweep at radix 16
 */
static void test_tan_radian_accuracy(void)
{
    double max_err_old = 0.0, max_err_new = 0.0;
    double sum_err_old = 0.0, sum_err_new = 0.0;
    int count = 0;
    int i;

    printf("## Tangent Radian Accuracy (10000 points, radix 16)\n\n");

    /* Sweep radians from -pi to pi in 10000 steps */
    for (i = 0; i < 10000; i++) {
        double angle = -M_PI + 2.0 * M_PI * (double)i / 10000.0;
        double ref = tan(angle);
        s32 rad16 = (s32)(angle * 65536.0);
        double old_val, new_val, err_old, err_new;

        if (fabs(ref) > 500.0) continue;

        old_val = fr2d(fr_tan(rad16, 16), 16);
        new_val = fr2d(fr_tan32(rad16, 16), 16);

        if (fabs(ref) > 0.001) {
            err_old = fabs((old_val - ref) / ref) * 100.0;
            err_new = fabs((new_val - ref) / ref) * 100.0;
        } else {
            err_old = fabs(old_val - ref) * 100.0;
            err_new = fabs(new_val - ref) * 100.0;
        }

        sum_err_old += err_old;
        sum_err_new += err_new;
        if (err_old > max_err_old) max_err_old = err_old;
        if (err_new > max_err_new) max_err_new = err_new;
        count++;
    }

    printf("| Metric         | Current (fr_tan)     | LUT32 (fr_tan32)     |\n");
    printf("|----------------|----------------------|----------------------|\n");
    printf("| Peak error (%%) |          %11.6f |          %11.6f |\n", max_err_old, max_err_new);
    printf("| Avg error (%%)  |          %11.6f |          %11.6f |\n", sum_err_old / count, sum_err_new / count);
    printf("| Points tested  |              %6d  |              %6d  |\n", count, count);
    printf("\n");
}

/*=======================================================
 * Test 3: Tangent integer-degree accuracy — 0..359 degrees
 */
static void test_tan_degree_accuracy(void)
{
    double max_err_old = 0.0, max_err_new = 0.0;
    double sum_err_old = 0.0, sum_err_new = 0.0;
    int count = 0;
    int deg;

    printf("## Tangent Integer-Degree Accuracy (360 degrees)\n\n");

    for (deg = 0; deg < 360; deg++) {
        double angle = (double)deg * M_PI / 180.0;
        double ref = tan(angle);
        double old_val, new_val, err_old, err_new;

        if (fabs(ref) > 500.0) continue;

        old_val = fr2d(FR_TanI((s16)deg), 16);
        new_val = fr2d(fr_tan_deg32((s16)deg, 0), 16);

        if (fabs(ref) > 0.001) {
            err_old = fabs((old_val - ref) / ref) * 100.0;
            err_new = fabs((new_val - ref) / ref) * 100.0;
        } else {
            err_old = fabs(old_val - ref) * 100.0;
            err_new = fabs(new_val - ref) * 100.0;
        }

        sum_err_old += err_old;
        sum_err_new += err_new;
        if (err_old > max_err_old) max_err_old = err_old;
        if (err_new > max_err_new) max_err_new = err_new;
        count++;
    }

    printf("| Metric         | Current (FR_TanI)    | LUT32 (fr_tan_deg32) |\n");
    printf("|----------------|----------------------|----------------------|\n");
    printf("| Peak error (%%) |          %11.6f |          %11.6f |\n", max_err_old, max_err_new);
    printf("| Avg error (%%)  |          %11.6f |          %11.6f |\n", sum_err_old / count, sum_err_new / count);
    printf("| Points tested  |              %6d  |              %6d  |\n", count, count);
    printf("\n");
}

/*=======================================================
 * Test 4: Tangent speed comparison (BAM)
 */
static void test_tan_speed(void)
{
    volatile s32 sink = 0;
    clock_t start, end;
    double old_ns, new_ns;
    int iters = 1000000;
    int i;

    printf("## Tangent Speed (%d iterations)\n\n", iters);

    /* Warm up */
    for (i = 0; i < 1000; i++) sink += fr_tan_bam((u16)i);

    start = clock();
    for (i = 0; i < iters; i++)
        sink += fr_tan_bam((u16)(i & 0xFFFF));
    end = clock();
    old_ns = (double)(end - start) / CLOCKS_PER_SEC * 1e9 / iters;

    start = clock();
    for (i = 0; i < iters; i++)
        sink += fr_tan_bam32((u16)(i & 0xFFFF));
    end = clock();
    new_ns = (double)(end - start) / CLOCKS_PER_SEC * 1e9 / iters;

    printf("| Metric         | Current (fr_tan_bam) | LUT32 (fr_tan_bam32) |\n");
    printf("|----------------|----------------------|----------------------|\n");
    printf("| ns/call        |          %11.1f |          %11.1f |\n", old_ns, new_ns);
    printf("\n");

    (void)sink;
}

/*=======================================================
 * Test 5: atan2 accuracy sweep — angles at multiple radii
 */
static void test_atan2_accuracy(void)
{
    double max_err_old = 0.0, max_err_new = 0.0;
    double sum_err_old = 0.0, sum_err_new = 0.0;
    int count = 0;
    int ri, ai;
    static const double radii[] = { 0.1, 1.0, 10.0, 100.0, 1000.0 };

    printf("## atan2 Accuracy Sweep (5 radii x 65536 angles)\n\n");

    for (ri = 0; ri < 5; ri++) {
        double r = radii[ri];
        for (ai = 0; ai < 65536; ai++) {
            double angle = (double)ai * 2.0 * M_PI / 65536.0 - M_PI;
            double fx = r * cos(angle);
            double fy = r * sin(angle);
            s32 x = (s32)(fx * 65536.0);
            s32 y = (s32)(fy * 65536.0);
            double ref = atan2(fy, fx);
            double old_val, new_val, err_old, err_new;

            /* Skip degenerate */
            if (x == 0 && y == 0) continue;

            old_val = fr2d(FR_atan2(y, x, 16), 16);
            new_val = fr2d(fr_atan2_32(y, x, 16), 16);

            /* Absolute error in radians, wrapped to [-pi, pi] */
            err_old = fabs(old_val - ref);
            err_new = fabs(new_val - ref);
            /* Handle wraparound near +/-pi: difference > pi means we
             * crossed the branch cut; true angular error is 2*pi - diff */
            if (err_old > M_PI) err_old = 2.0 * M_PI - err_old;
            if (err_new > M_PI) err_new = 2.0 * M_PI - err_new;
            /* Convert to % of pi for reporting */
            err_old = err_old / M_PI * 100.0;
            err_new = err_new / M_PI * 100.0;

            sum_err_old += err_old;
            sum_err_new += err_new;
            if (err_old > max_err_old) max_err_old = err_old;
            if (err_new > max_err_new) max_err_new = err_new;
            count++;
        }
    }

    printf("| Metric              | Current (FR_atan2)   | LUT32 (fr_atan2_32)  |\n");
    printf("|---------------------|----------------------|----------------------|\n");
    printf("| Peak error (%% of pi)|          %11.6f |          %11.6f |\n", max_err_old, max_err_new);
    printf("| Avg error (%% of pi) |          %11.6f |          %11.6f |\n", sum_err_old / count, sum_err_new / count);
    printf("| Points tested       |              %6d  |              %6d  |\n", count, count);
    printf("\n");
}

/*=======================================================
 * Test 6: atan2 speed comparison
 */
static void test_atan2_speed(void)
{
    volatile s32 sink = 0;
    clock_t start, end;
    double old_ns, new_ns;
    int iters = 500000;
    int i;

    printf("## atan2 Speed (%d iterations)\n\n", iters);

    /* Pre-compute some x,y pairs */
    s32 xs[256], ys[256];
    for (i = 0; i < 256; i++) {
        double angle = (double)i * 2.0 * M_PI / 256.0;
        xs[i] = (s32)(10.0 * cos(angle) * 65536.0);
        ys[i] = (s32)(10.0 * sin(angle) * 65536.0);
    }

    /* Warm up */
    for (i = 0; i < 256; i++) sink += FR_atan2(ys[i], xs[i], 16);

    start = clock();
    for (i = 0; i < iters; i++)
        sink += FR_atan2(ys[i & 0xFF], xs[i & 0xFF], 16);
    end = clock();
    old_ns = (double)(end - start) / CLOCKS_PER_SEC * 1e9 / iters;

    start = clock();
    for (i = 0; i < iters; i++)
        sink += fr_atan2_32(ys[i & 0xFF], xs[i & 0xFF], 16);
    end = clock();
    new_ns = (double)(end - start) / CLOCKS_PER_SEC * 1e9 / iters;

    printf("| Metric         | Current (FR_atan2)   | LUT32 (fr_atan2_32)  |\n");
    printf("|----------------|----------------------|----------------------|\n");
    printf("| ns/call        |          %11.1f |          %11.1f |\n", old_ns, new_ns);
    printf("\n");

    (void)sink;
}

/*=======================================================
 * Test 7: Quick spot checks for correctness
 */
static int test_spot_checks(void)
{
    int fails = 0;
    s32 v;

    printf("## Spot Checks\n\n");

    /* tan(0) = 0 */
    v = fr_tan_bam32(0);
    if (v != 0) { printf("  FAIL: tan_bam32(0) = %d, expected 0\n", v); fails++; }

    /* tan(45 deg) = 1.0 = 65536 in s15.16 */
    v = fr_tan_bam32(0x2000);  /* 45 deg = 8192 BAM */
    if (abs(v - 65536) > 2) { printf("  FAIL: tan_bam32(45deg) = %d, expected ~65536\n", v); fails++; }

    /* tan(180 deg) = 0 */
    v = fr_tan_bam32(0x8000);
    if (v != 0) { printf("  FAIL: tan_bam32(180deg) = %d, expected 0\n", v); fails++; }

    /* tan(90 deg) = pole */
    v = fr_tan_bam32(0x4000);
    if (v != FR_TRIG_MAXVAL) { printf("  FAIL: tan_bam32(90deg) = %d, expected %d\n", v, FR_TRIG_MAXVAL); fails++; }

    /* tan(270 deg) = -pole */
    v = fr_tan_bam32(0xC000);
    if (v != -FR_TRIG_MAXVAL) { printf("  FAIL: tan_bam32(270deg) = %d, expected %d\n", v, -FR_TRIG_MAXVAL); fails++; }

    /* Radian wrapper: tan(pi/4) = 1.0 */
    {
        s32 pi_4 = (s32)(M_PI / 4.0 * 65536.0);
        v = fr_tan32(pi_4, 16);
        if (abs(v - 65536) > 100) { printf("  FAIL: tan32(pi/4) = %d (%.6f), expected ~65536\n", v, fr2d(v, 16)); fails++; }
    }

    /* Degree wrapper: tan(45) = 1.0 */
    v = fr_tan_deg32(45, 0);
    if (abs(v - 65536) > 100) { printf("  FAIL: tan_deg32(45) = %d (%.6f), expected ~65536\n", v, fr2d(v, 16)); fails++; }

    /* Degree wrapper: tan(0) = 0 */
    v = fr_tan_deg32(0, 0);
    if (v != 0) { printf("  FAIL: tan_deg32(0) = %d, expected 0\n", v); fails++; }

    /* atan2(0, 1) = 0 */
    v = fr_atan2_32(0, 65536, 16);
    if (v != 0) { printf("  FAIL: atan2_32(0,1) = %d, expected 0\n", v); fails++; }

    /* atan2(1, 0) = pi/2 */
    {
        s32 expected = FR_CHRDX(FR_kQ2RAD, FR_kPREC, 16);
        v = fr_atan2_32(65536, 0, 16);
        if (abs(v - expected) > 2) { printf("  FAIL: atan2_32(1,0) = %d, expected ~%d\n", v, expected); fails++; }
    }

    /* atan2(1, 1) = pi/4 */
    {
        double ref = M_PI / 4.0;
        s32 expected = (s32)(ref * 65536.0);
        v = fr_atan2_32(65536, 65536, 16);
        if (abs(v - expected) > 200) { printf("  FAIL: atan2_32(1,1) = %d (%.6f), expected ~%d (%.6f)\n",
            v, fr2d(v, 16), expected, ref); fails++; }
    }

    /* atan2(-1, -1) = -3*pi/4 */
    {
        double ref = -3.0 * M_PI / 4.0;
        s32 expected = (s32)(ref * 65536.0);
        v = fr_atan2_32(-65536, -65536, 16);
        if (abs(v - expected) > 200) { printf("  FAIL: atan2_32(-1,-1) = %d (%.6f), expected ~%d (%.6f)\n",
            v, fr2d(v, 16), expected, ref); fails++; }
    }

    if (fails == 0)
        printf("  All spot checks PASSED\n");
    else
        printf("  %d spot check(s) FAILED\n", fails);
    printf("\n");

    return fails;
}

/*=======================================================
 * Main
 */
int main(void)
{
    int fails;

    printf("# FR_tan32 Head-to-Head Comparison Report\n\n");

    fails = test_spot_checks();
    test_tan_bam_accuracy();
    test_tan_radian_accuracy();
    test_tan_degree_accuracy();
    test_tan_speed();
    test_atan2_accuracy();
    test_atan2_speed();

    printf("## Summary\n\n");
    printf("Design notes:\n");
    printf("  - tan: sin/cos from the existing 129-entry cosine table (258B, already in ROM)\n");
    printf("    No extra tan table needed for the forward path.  One s64 division per call.\n");
    printf("    Current uses octant table (130B) + reciprocal division for [45,90] deg.\n\n");
    printf("  - atan2: binary search on 129-entry u32 tan table (516B) + quadrant mapping\n");
    printf("    Current uses hypot_fast8 -> asin/acos chain (more code, no extra table)\n\n");
    printf("  - Tan table (516B) needed only for atan2.  Could be omitted if atan2 not used.\n\n");

    return fails ? 1 : 0;
}
