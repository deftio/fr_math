/**
 * test_sweep_csv.c — emit CSV + summary for all 65536 BAM values
 *
 * Compares 3 tan implementations:
 *   fr_tan_bam (old):   65-entry u16 octant table + reciprocal
 *   fr_tan_bam32_d64:   sin/cos from 129-entry cos table, s64 div
 *   fr_tan_bam32 (new): direct 65-entry u32 tan table lookup, no div
 *
 * Ground truth clamped to ±SAT_MAX for fair pole comparison.
 *
 * Output: build/tan_sweep.csv
 */

#include <stdio.h>
#include <math.h>
#include <time.h>
#include "FR_math.h"
#include "FR_trig_table.h"
#include "FR_tan_table.h"

extern s32 fr_tan_bam32(u16 bam);
extern s32 fr_tan_bam32_d64(u16 bam);

#define SAT_MAX (32767.999984741211)

static double to_dbl(s32 v) { return (double)v / 65536.0; }

static double clamp(double v)
{
    if (v >  SAT_MAX) return  SAT_MAX;
    if (v < -SAT_MAX) return -SAT_MAX;
    return v;
}

static double err_pct(double ref, double got)
{
    if (fabs(ref) >= SAT_MAX && fabs(got) >= SAT_MAX)
        return 0.0;
    double ae = fabs(got - ref);
    if (fabs(ref) >= 0.01)
        return (ae / fabs(ref)) * 100.0;
    return ae * 100.0;
}

static double now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e9 + (double)ts.tv_nsec;
}

int main(void)
{
    FILE *fp = fopen("build/tan_sweep.csv", "w");
    if (!fp) { perror("fopen"); return 1; }

    fprintf(fp, "bam,degrees,tan_truth,"
                "tan_old,tan_d64,tan_direct,"
                "err_old,err_d64,err_direct\n");

    for (long b = 0; b < 65536; b++) {
        u16 bam = (u16)b;
        double deg = (double)bam * 360.0 / 65536.0;
        double rad = (double)bam * 2.0 * M_PI / 65536.0;
        double truth = clamp(tan(rad));

        double v_old    = to_dbl(fr_tan_bam(bam));
        double v_d64    = to_dbl(fr_tan_bam32_d64(bam));
        double v_direct = to_dbl(fr_tan_bam32(bam));

        fprintf(fp, "%u,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f\n",
                bam, deg, truth,
                v_old, v_d64, v_direct,
                err_pct(truth, v_old),
                err_pct(truth, v_d64),
                err_pct(truth, v_direct));
    }

    fclose(fp);

    /* Timing */
    volatile s32 sink = 0;
    double t0, t1;
    long N = 65536;

    t0 = now_ns();
    for (long b = 0; b < N; b++) sink += fr_tan_bam((u16)b);
    t1 = now_ns();
    double ns_old = (t1 - t0) / N;

    t0 = now_ns();
    for (long b = 0; b < N; b++) sink += fr_tan_bam32_d64((u16)b);
    t1 = now_ns();
    double ns_d64 = (t1 - t0) / N;

    t0 = now_ns();
    for (long b = 0; b < N; b++) sink += fr_tan_bam32((u16)b);
    t1 = now_ns();
    double ns_direct = (t1 - t0) / N;

    (void)sink;

    /* Stats */
    printf("Wrote build/tan_sweep.csv (65536 rows)\n\n");

    double peak_old = 0, peak_d64 = 0, peak_dir = 0;
    double sum_old = 0, sum_d64 = 0, sum_dir = 0;
    int peak_bam_old = 0, peak_bam_d64 = 0, peak_bam_dir = 0;

    for (long b = 0; b < 65536; b++) {
        u16 bam = (u16)b;
        double rad = (double)bam * 2.0 * M_PI / 65536.0;
        double truth = clamp(tan(rad));

        double e_old = err_pct(truth, to_dbl(fr_tan_bam(bam)));
        double e_d64 = err_pct(truth, to_dbl(fr_tan_bam32_d64(bam)));
        double e_dir = err_pct(truth, to_dbl(fr_tan_bam32(bam)));

        sum_old += e_old; sum_d64 += e_d64; sum_dir += e_dir;
        if (e_old > peak_old) { peak_old = e_old; peak_bam_old = bam; }
        if (e_d64 > peak_d64) { peak_d64 = e_d64; peak_bam_d64 = bam; }
        if (e_dir > peak_dir) { peak_dir = e_dir; peak_bam_dir = bam; }
    }

    printf("| %-24s | %5s | %10s | %10s | %7s | %-24s |\n",
           "Implementation", "Table", "Peak Err", "Avg Err", "ns/call", "Peak At");
    printf("| %-24s | %5s | %10s | %10s | %7s | %-24s |\n",
           "------------------------", "-----", "----------", "----------", "-------",
           "------------------------");
    printf("| %-24s | %5s | %9.4f%% | %9.5f%% | %5.1f   | BAM %5d (%6.2f deg) |\n",
           "fr_tan_bam (old)", "65u16",
           peak_old, sum_old / 65536, ns_old,
           peak_bam_old, peak_bam_old * 360.0 / 65536.0);
    printf("| %-24s | %5s | %9.4f%% | %9.5f%% | %5.1f   | BAM %5d (%6.2f deg) |\n",
           "fr_tan_bam32_d64 (s/c)", "none",
           peak_d64, sum_d64 / 65536, ns_d64,
           peak_bam_d64, peak_bam_d64 * 360.0 / 65536.0);
    printf("| %-24s | %5s | %9.4f%% | %9.5f%% | %5.1f   | BAM %5d (%6.2f deg) |\n",
           "fr_tan_bam32 (direct)", "65u32",
           peak_dir, sum_dir / 65536, ns_direct,
           peak_bam_dir, peak_bam_dir * 360.0 / 65536.0);

    printf("\nOld:    65-entry u16 octant table + reciprocal (div in 2nd octant).\n");
    printf("d64:    sin/cos via 129-entry cos table, always s64 div.\n");
    printf("Direct: 65-entry u32 quadrant tan table, lerp with shift, NO div.\n");

    printf("\nDone.\n");
    return 0;
}
