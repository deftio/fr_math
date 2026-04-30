/*
 * test_tan32_sweep.c - Comprehensive -65536..+65536 sweep for all tan/atan functions
 *
 * Generates a single comparison table: old vs new, BAM / radian / degree,
 * with peak error, avg error, and speed for each function.
 *
 * Compile:
 *   cc -Isrc -Wall -Os src/FR_tan32.c src/FR_math.c tests/test_tan32_sweep.c -lm -o build/test_tan32_sweep
 *
 * @author M A Chatterjee <deftio [at] deftio [dot] com>
 */

#include <stdio.h>
#include <math.h>
#include <time.h>
#include <stdlib.h>
#include "../src/FR_math.h"

extern s32 fr_tan_bam32(u16 bam);
extern s32 fr_tan32(s32 rad, u16 radix);
extern s32 fr_tan_deg32(s32 deg, u16 radix);
extern s32 fr_atan2_32(s32 y, s32 x, u16 out_radix);

static double fr2d(s32 val, int radix) {
    return (double)val / (double)(1L << radix);
}

typedef struct {
    double peak_err;
    double sum_err;
    int    count;
} stats_t;

static void stats_init(stats_t *s) { s->peak_err = 0; s->sum_err = 0; s->count = 0; }
static void stats_add(stats_t *s, double err) {
    if (err > s->peak_err) s->peak_err = err;
    s->sum_err += err;
    s->count++;
}
static double stats_avg(stats_t *s) { return s->count > 0 ? s->sum_err / s->count : 0; }

/*=======================================================
 * Speed measurement helper
 */
static double measure_ns(void (*fn)(volatile s32 *sink, int n), int n) {
    volatile s32 sink = 0;
    clock_t start, end;
    /* warm up */
    fn(&sink, n / 10);
    start = clock();
    fn(&sink, n);
    end = clock();
    return (double)(end - start) / CLOCKS_PER_SEC * 1e9 / n;
}

/* Speed test functions */
static void speed_tan_bam_old(volatile s32 *sink, int n) {
    int i; for (i = 0; i < n; i++) *sink += fr_tan_bam((u16)(i & 0xFFFF));
}
static void speed_tan_bam_new(volatile s32 *sink, int n) {
    int i; for (i = 0; i < n; i++) *sink += fr_tan_bam32((u16)(i & 0xFFFF));
}
static void speed_tan_rad_old(volatile s32 *sink, int n) {
    int i; for (i = 0; i < n; i++) *sink += fr_tan((s32)((i * 7) - n * 3), 16);
}
static void speed_tan_rad_new(volatile s32 *sink, int n) {
    int i; for (i = 0; i < n; i++) *sink += fr_tan32((s32)((i * 7) - n * 3), 16);
}
static void speed_tan_deg_old(volatile s32 *sink, int n) {
    int i; for (i = 0; i < n; i++) *sink += FR_TanI((s16)(i % 360));
}
static void speed_tan_deg_new(volatile s32 *sink, int n) {
    int i; for (i = 0; i < n; i++) *sink += fr_tan_deg32((s16)(i % 360), 0);
}

static s32 g_xs[256], g_ys[256];
static void init_atan_data(void) {
    int i;
    for (i = 0; i < 256; i++) {
        double a = (double)i * 2.0 * M_PI / 256.0;
        g_xs[i] = (s32)(10.0 * cos(a) * 65536.0);
        g_ys[i] = (s32)(10.0 * sin(a) * 65536.0);
    }
}
static void speed_atan2_old(volatile s32 *sink, int n) {
    int i; for (i = 0; i < n; i++) *sink += FR_atan2(g_ys[i & 0xFF], g_xs[i & 0xFF], 16);
}
static void speed_atan2_new(volatile s32 *sink, int n) {
    int i; for (i = 0; i < n; i++) *sink += fr_atan2_32(g_ys[i & 0xFF], g_xs[i & 0xFF], 16);
}
static void speed_atan_old(volatile s32 *sink, int n) {
    int i; for (i = 0; i < n; i++) *sink += FR_atan((s32)((i * 13) - n * 6), 16, 16);
}
static void speed_atan_new(volatile s32 *sink, int n) {
    /* FR_atan(x, r, or) = FR_atan2(x, 1<<r, or), use fr_atan2_32 */
    int i; for (i = 0; i < n; i++) *sink += fr_atan2_32((s32)((i * 13) - n * 6), 65536, 16);
}

/*=======================================================
 * Tangent sweeps
 */
static void sweep_tan_bam(stats_t *old_s, stats_t *new_s)
{
    s32 bam;
    stats_init(old_s);
    stats_init(new_s);

    for (bam = 0; bam < 65536; bam++) {
        double angle = (double)bam * 2.0 * M_PI / 65536.0;
        double ref = tan(angle);
        double ov, nv, oe, ne;
        if (fabs(ref) > 500.0) continue;

        ov = fr2d(fr_tan_bam((u16)bam), 16);
        nv = fr2d(fr_tan_bam32((u16)bam), 16);

        if (fabs(ref) > 0.001) {
            oe = fabs((ov - ref) / ref) * 100.0;
            ne = fabs((nv - ref) / ref) * 100.0;
        } else {
            oe = fabs(ov - ref) * 100.0;
            ne = fabs(nv - ref) * 100.0;
        }
        stats_add(old_s, oe);
        stats_add(new_s, ne);
    }
}

static void sweep_tan_rad(stats_t *old_s, stats_t *new_s)
{
    s32 rad16;
    stats_init(old_s);
    stats_init(new_s);

    /* Sweep s15.16 radians from -65536 to +65535 (= -1.0 to +1.0 rad ≈ ±57 deg).
     * Step by 1 LSB = full 131072-point sweep. */
    for (rad16 = -65536; rad16 <= 65535; rad16++) {
        double angle = (double)rad16 / 65536.0;
        double ref = tan(angle);
        double ov, nv, oe, ne;
        if (fabs(ref) > 500.0) continue;

        ov = fr2d(fr_tan(rad16, 16), 16);
        nv = fr2d(fr_tan32(rad16, 16), 16);

        if (fabs(ref) > 0.001) {
            oe = fabs((ov - ref) / ref) * 100.0;
            ne = fabs((nv - ref) / ref) * 100.0;
        } else {
            oe = fabs(ov - ref) * 100.0;
            ne = fabs(nv - ref) * 100.0;
        }
        stats_add(old_s, oe);
        stats_add(new_s, ne);
    }
}

static void sweep_tan_deg(stats_t *old_s, stats_t *new_s)
{
    s16 deg;
    stats_init(old_s);
    stats_init(new_s);

    for (deg = -180; deg <= 179; deg++) {
        double ref = tan((double)deg * M_PI / 180.0);
        double ov, nv, oe, ne;
        if (fabs(ref) > 500.0) continue;

        ov = fr2d(FR_TanI(deg), 16);
        nv = fr2d(fr_tan_deg32(deg, 0), 16);

        if (fabs(ref) > 0.001) {
            oe = fabs((ov - ref) / ref) * 100.0;
            ne = fabs((nv - ref) / ref) * 100.0;
        } else {
            oe = fabs(ov - ref) * 100.0;
            ne = fabs(nv - ref) * 100.0;
        }
        stats_add(old_s, oe);
        stats_add(new_s, ne);
    }
}

/*=======================================================
 * Atan sweeps
 */
static void sweep_atan2(stats_t *old_s, stats_t *new_s)
{
    int ri, ai;
    static const double radii[] = { 0.1, 1.0, 10.0, 100.0, 1000.0 };
    stats_init(old_s);
    stats_init(new_s);

    for (ri = 0; ri < 5; ri++) {
        double r = radii[ri];
        for (ai = 0; ai < 65536; ai++) {
            double angle = (double)ai * 2.0 * M_PI / 65536.0 - M_PI;
            double fx = r * cos(angle), fy = r * sin(angle);
            s32 x = (s32)(fx * 65536.0), y = (s32)(fy * 65536.0);
            double ref = atan2(fy, fx);
            double ov, nv, oe, ne;
            if (x == 0 && y == 0) continue;

            ov = fr2d(FR_atan2(y, x, 16), 16);
            nv = fr2d(fr_atan2_32(y, x, 16), 16);

            oe = fabs(ov - ref); ne = fabs(nv - ref);
            if (oe > M_PI) oe = 2.0 * M_PI - oe;
            if (ne > M_PI) ne = 2.0 * M_PI - ne;
            oe = oe / M_PI * 100.0;
            ne = ne / M_PI * 100.0;

            stats_add(old_s, oe);
            stats_add(new_s, ne);
        }
    }
}

static void sweep_atan(stats_t *old_s, stats_t *new_s)
{
    s32 x16;
    stats_init(old_s);
    stats_init(new_s);

    /* Sweep atan input from -65536 to +65535 (= -1.0 to +1.0 in s15.16).
     * Step by 8 to keep runtime reasonable (16384 points).
     * Error metric: absolute angular error as % of pi/2 (atan range). */
    for (x16 = -65536; x16 <= 65535; x16 += 8) {
        double xf = (double)x16 / 65536.0;
        double ref = atan(xf);
        double ov, nv, oe, ne;

        ov = fr2d(FR_atan(x16, 16, 16), 16);
        nv = fr2d(fr_atan2_32(x16, 65536, 16), 16);

        /* Use absolute angular error / (pi/2) * 100, same approach as atan2 */
        oe = fabs(ov - ref) / (M_PI / 2.0) * 100.0;
        ne = fabs(nv - ref) / (M_PI / 2.0) * 100.0;

        stats_add(old_s, oe);
        stats_add(new_s, ne);
    }
}

/*=======================================================
 * Main
 */
int main(void)
{
    stats_t old_s, new_s;
    double old_ns, new_ns;
    int N = 1000000;

    init_atan_data();

    printf("# Comprehensive Function Comparison: Old vs New\n\n");
    printf("Sweep range: full domain for each input type\n");
    printf("Error metric: relative %% (or absolute*100 near zero)\n");
    printf("Speed: ns/call on this platform\n\n");

    printf("## Tangent Functions\n\n");
    printf("| Function           | Impl  | Sweep Range       | Points  | Peak Err %%  | Avg Err %%   | ns/call |\n");
    printf("|--------------------|-------|-------------------|---------|-------------|-------------|--------:|\n");

    sweep_tan_bam(&old_s, &new_s);
    old_ns = measure_ns(speed_tan_bam_old, N);
    new_ns = measure_ns(speed_tan_bam_new, N);
    printf("| tan_bam (BAM)      | OLD   | 0..65535 BAM      | %7d | %11.6f | %11.6f | %5.1f   |\n",
        old_s.count, old_s.peak_err, stats_avg(&old_s), old_ns);
    printf("| tan_bam32 (BAM)    | NEW   | 0..65535 BAM      | %7d | %11.6f | %11.6f | %5.1f   |\n",
        new_s.count, new_s.peak_err, stats_avg(&new_s), new_ns);

    sweep_tan_rad(&old_s, &new_s);
    old_ns = measure_ns(speed_tan_rad_old, N);
    new_ns = measure_ns(speed_tan_rad_new, N);
    printf("| fr_tan (rad@r16)   | OLD   | -65536..+65535 r16| %7d | %11.6f | %11.6f | %5.1f   |\n",
        old_s.count, old_s.peak_err, stats_avg(&old_s), old_ns);
    printf("| fr_tan32 (rad@r16) | NEW   | -65536..+65535 r16| %7d | %11.6f | %11.6f | %5.1f   |\n",
        new_s.count, new_s.peak_err, stats_avg(&new_s), new_ns);

    sweep_tan_deg(&old_s, &new_s);
    old_ns = measure_ns(speed_tan_deg_old, N);
    new_ns = measure_ns(speed_tan_deg_new, N);
    printf("| FR_TanI (deg)      | OLD   | -180..+179 deg    | %7d | %11.6f | %11.6f | %5.1f   |\n",
        old_s.count, old_s.peak_err, stats_avg(&old_s), old_ns);
    printf("| fr_tan_deg32 (deg) | NEW   | -180..+179 deg    | %7d | %11.6f | %11.6f | %5.1f   |\n",
        new_s.count, new_s.peak_err, stats_avg(&new_s), new_ns);

    printf("\n## Inverse Tangent Functions\n\n");
    printf("| Function           | Impl  | Sweep Range       | Points  | Peak Err %%  | Avg Err %%   | ns/call |\n");
    printf("|--------------------|-------|-------------------|---------|-------------|-------------|--------:|\n");

    sweep_atan2(&old_s, &new_s);
    old_ns = measure_ns(speed_atan2_old, N / 2);
    new_ns = measure_ns(speed_atan2_new, N / 2);
    printf("| FR_atan2 (s15.16)  | OLD   | 5 radii x 65536   | %7d | %11.6f | %11.6f | %5.1f   |\n",
        old_s.count, old_s.peak_err, stats_avg(&old_s), old_ns);
    printf("| fr_atan2_32(s15.16)| NEW   | 5 radii x 65536   | %7d | %11.6f | %11.6f | %5.1f   |\n",
        new_s.count, new_s.peak_err, stats_avg(&new_s), new_ns);

    sweep_atan(&old_s, &new_s);
    old_ns = measure_ns(speed_atan_old, N / 2);
    new_ns = measure_ns(speed_atan_new, N / 2);
    printf("| FR_atan (s15.16)   | OLD   | -65536..+65535 /8 | %7d | %11.6f | %11.6f | %5.1f   |\n",
        old_s.count, old_s.peak_err, stats_avg(&old_s), old_ns);
    printf("| atan2_32(x,1) eq.  | NEW   | -65536..+65535 /8 | %7d | %11.6f | %11.6f | %5.1f   |\n",
        new_s.count, new_s.peak_err, stats_avg(&new_s), new_ns);

    printf("\n## Notes\n\n");
    printf("- BAM sweep: 0..65535 (full circle, excludes |tan|>500 near poles)\n");
    printf("- Radian sweep: -65536..+65535 at radix 16 = -1.0..+1.0 rad = +/-57.3 deg\n");
    printf("- Degree sweep: -180..+179 integer degrees\n");
    printf("- atan2 error: %% of pi (angular error / pi * 100)\n");
    printf("- atan error: absolute angular error / (pi/2) * 100%%\n");
    printf("- atan2_32(x,1) is used as the NEW atan since it's equivalent to atan(x)\n");

    return 0;
}
