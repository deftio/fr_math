/**
 * test_full_sweep.c — exhaustive error sweep for cos and tan (old & new)
 *
 * Three independent sweeps, each in its native input domain:
 *   BAM:    all 65536 u16 values (0..65535)
 *   Radian: every s15.16 LSB from -2pi to +2pi (~823k values)
 *   Degree: fr_tan_deg32(s32,16) at s15.16, 1/1024 deg steps, ±360 deg (~738k)
 *           FR_Tan(s16,6) at s9.6 for old (s16 limits range)
 *           FR_TanI(deg) tested at integer-degree-aligned subset
 *
 * Error metrics:
 *   cos: % of full scale (1.0).  |comp/65536 - ref| * 100
 *   tan: relative % when |ref| >= 0.01, else absolute % of 1.0
 *        Skipped when |ref| > 1000 (near-pole, unrepresentable in s15.16)
 *
 * Also reports average ns/call for each function.
 */

#include <stdio.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include "FR_math.h"
#include "FR_trig_table.h"
#include "FR_tan_table.h"

/* FR_tan32.c functions */
extern s32 fr_tan_bam32(u16 bam);
extern s32 fr_tan32(s32 rad, u16 radix);
extern s32 fr_tan_deg32(s32 deg, u16 radix);

/* ── sweep accumulator ─────────────────────────────── */

typedef struct {
    const char *name;
    double peak_err;
    double ref_at_peak;
    s32    val_at_peak;
    double sum_err;
    long   count;
    char   peak_label[64];
    double total_ns;
    long   time_count;
} sweep_t;

static void sw_init(sweep_t *s, const char *name)
{
    memset(s, 0, sizeof(*s));
    s->name = name;
}

static void sw_cos(sweep_t *s, double ref, s32 comp, const char *label)
{
    double comp_dbl = (double)comp / 65536.0;
    double pct = fabs(comp_dbl - ref) * 100.0;
    s->sum_err += pct;
    s->count++;
    if (pct > s->peak_err) {
        s->peak_err    = pct;
        s->ref_at_peak = ref;
        s->val_at_peak = comp;
        strncpy(s->peak_label, label, sizeof(s->peak_label) - 1);
    }
}

#define TAN_CLIP 1000.0
#define TAN_ZERO 0.01

static void sw_tan(sweep_t *s, double ref, s32 comp, const char *label)
{
    if (fabs(ref) > TAN_CLIP) return;
    double comp_dbl = (double)comp / 65536.0;
    double abs_err  = fabs(comp_dbl - ref);
    double pct = (fabs(ref) >= TAN_ZERO)
                 ? (abs_err / fabs(ref)) * 100.0
                 : abs_err * 100.0;
    s->sum_err += pct;
    s->count++;
    if (pct > s->peak_err) {
        s->peak_err    = pct;
        s->ref_at_peak = ref;
        s->val_at_peak = comp;
        strncpy(s->peak_label, label, sizeof(s->peak_label) - 1);
    }
}

static double now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e9 + (double)ts.tv_nsec;
}

static void sw_header(void)
{
    printf("| %-26s | %10s | %10s | %7s | %-30s | %12s | %10s |\n",
           "Function", "Peak Err", "Avg Err", "ns/call",
           "Peak At", "Ref Value", "Got (s32)");
    printf("| %-26s | %10s | %10s | %7s | %-30s | %12s | %10s |\n",
           "--------------------------", "----------", "----------", "-------",
           "------------------------------", "------------", "----------");
}

static void sw_print(const sweep_t *s)
{
    double avg = (s->count > 0) ? s->sum_err / (double)s->count : 0.0;
    double ns  = (s->time_count > 0) ? s->total_ns / (double)s->time_count : 0.0;
    printf("| %-26s | %9.4f%% | %9.5f%% | %5.1f   | %-30s | %12.6f | %10d |\n",
           s->name, s->peak_err, avg, ns, s->peak_label,
           s->ref_at_peak, (int)s->val_at_peak);
}

/* ════════════════════════════════════════════════════════
 * BAM sweep: all 65536 u16 values
 * ════════════════════════════════════════════════════════ */
static void sweep_bam(void)
{
    sweep_t cos_old, tan_old, tan_new;
    sw_init(&cos_old,  "fr_cos_bam");
    sw_init(&tan_old,  "fr_tan_bam (old)");
    sw_init(&tan_new,  "fr_tan_bam32 (new)");

    for (long b = 0; b < 65536; b++) {
        u16 bam = (u16)b;
        double rad = (double)bam * 2.0 * M_PI / 65536.0;
        char label[64];
        snprintf(label, sizeof(label), "BAM %5u (%7.2f deg)",
                 bam, (double)bam * 360.0 / 65536.0);

        sw_cos(&cos_old, cos(rad), fr_cos_bam(bam), label);
        sw_tan(&tan_old, tan(rad), fr_tan_bam(bam), label);
        sw_tan(&tan_new, tan(rad), fr_tan_bam32(bam), label);
    }

    /* timing */
    {
        volatile s32 sink = 0;
        double t0, t1;
        long N = 65536;

        t0 = now_ns();
        for (long b = 0; b < N; b++) sink += fr_cos_bam((u16)b);
        t1 = now_ns();
        cos_old.total_ns = t1 - t0; cos_old.time_count = N;

        t0 = now_ns();
        for (long b = 0; b < N; b++) sink += fr_tan_bam((u16)b);
        t1 = now_ns();
        tan_old.total_ns = t1 - t0; tan_old.time_count = N;

        t0 = now_ns();
        for (long b = 0; b < N; b++) sink += fr_tan_bam32((u16)b);
        t1 = now_ns();
        tan_new.total_ns = t1 - t0; tan_new.time_count = N;

        (void)sink;
    }

    printf("### BAM domain — all 65536 u16 values\n\n");
    sw_header();
    sw_print(&cos_old);
    sw_print(&tan_old);
    sw_print(&tan_new);
    printf("\ntan samples: old=%ld, new=%ld (rest skipped near poles)\n\n",
           tan_old.count, tan_new.count);
}

/* ════════════════════════════════════════════════════════
 * Radian sweep: every s15.16 LSB from -2pi to +2pi
 * ════════════════════════════════════════════════════════ */
static void sweep_rad(void)
{
    sweep_t cos_old, tan_old, tan_new;
    sw_init(&cos_old,  "fr_cos (s15.16)");
    sw_init(&tan_old,  "fr_tan (s15.16)");
    sw_init(&tan_new,  "fr_tan32 (s15.16)");

    s32 two_pi = (s32)(2.0 * M_PI * 65536.0 + 0.5);  /* 411775 */
    long total = 0;

    for (s32 r = -two_pi; r <= two_pi; r++) {
        double rad = (double)r / 65536.0;
        char label[64];
        snprintf(label, sizeof(label), "r16=%d (%.4f rad)", r, rad);

        sw_cos(&cos_old, cos(rad), fr_cos(r, 16), label);
        sw_tan(&tan_old, tan(rad), fr_tan(r, 16), label);
        sw_tan(&tan_new, tan(rad), fr_tan32(r, 16), label);
        total++;
    }

    /* timing */
    {
        volatile s32 sink = 0;
        double t0, t1;
        long N = 65536;
        s32 step = (2 * two_pi) / N;
        if (step < 1) step = 1;

        t0 = now_ns();
        for (s32 r = -two_pi; r <= two_pi; r += step) sink += fr_cos(r, 16);
        t1 = now_ns();
        cos_old.total_ns = t1 - t0; cos_old.time_count = N;

        t0 = now_ns();
        for (s32 r = -two_pi; r <= two_pi; r += step) sink += fr_tan(r, 16);
        t1 = now_ns();
        tan_old.total_ns = t1 - t0; tan_old.time_count = N;

        t0 = now_ns();
        for (s32 r = -two_pi; r <= two_pi; r += step) sink += fr_tan32(r, 16);
        t1 = now_ns();
        tan_new.total_ns = t1 - t0; tan_new.time_count = N;

        (void)sink;
    }

    printf("### Radian domain — every s15.16 LSB, -2pi..+2pi (%ld values)\n\n", total);
    sw_header();
    sw_print(&cos_old);
    sw_print(&tan_old);
    sw_print(&tan_new);
    printf("\ntan samples: old=%ld, new=%ld\n\n", tan_old.count, tan_new.count);
}

/* ════════════════════════════════════════════════════════
 * Degree sweep: all 65536 s16 values at radix 6 (s9.6)
 *   s15.16 degrees: every LSB from -360*65536 to +360*65536 (~823k values)
 *   FR_Tan(deg,16) — old, s16 input limits to ±0.5 deg (too narrow!)
 *   fr_tan_deg32(deg,16) — new, s32 input, full s15.16 range
 *   FR_TanI(deg) — integer degrees (tested at integer-aligned subset)
 *
 *   NOTE: FR_Tan still takes s16, so its s15.16 sweep only covers ±0.5 deg.
 *   To get a fair comparison we ALSO test FR_Tan at radix=6 (s9.6, ±512 deg).
 * ════════════════════════════════════════════════════════ */
static void sweep_deg(void)
{
    sweep_t cos_old, tan_old_s16, tan_new_full, tan_old_int;
    sw_init(&cos_old,      "FR_Cos (s9.6 deg)");
    sw_init(&tan_old_s16,  "FR_Tan (s9.6 deg, s16)");
    sw_init(&tan_new_full, "fr_tan_deg32 (s15.16 deg)");
    sw_init(&tan_old_int,  "FR_TanI (int deg)");

    /* New path: s15.16 degrees, every LSB from -360 to +360.
     * 360 * 65536 = 23592960.  Total ~47M values — too many.
     * Use same density as radian sweep: ~823k values.
     * -360..+360 deg = 720 deg range.  823551 / 720 ≈ 1144 steps/deg.
     * That's close to radix=10 (1024 steps/deg).  Use radix=16 with
     * step = 65536/1024 = 64 to get ~720k values. */
    s32 deg360_s16 = 360L * 65536;
    s32 step_new = 64;  /* every 64th LSB of s15.16 = 1/1024 deg */
    long total_new = 0;

    for (s32 d = -deg360_s16; d <= deg360_s16; d += step_new) {
        double deg_dbl = (double)d / 65536.0;
        double rad     = deg_dbl * M_PI / 180.0;
        char label[64];
        snprintf(label, sizeof(label), "d16=%d (%.4f deg)", (int)d, deg_dbl);

        double rt = tan(rad);
        sw_tan(&tan_new_full, rt, fr_tan_deg32(d, 16), label);

        /* FR_TanI at integer-degree subset */
        if (d % 65536 == 0) {
            s16 ideg = (s16)(d / 65536);
            char ilabel[64];
            snprintf(ilabel, sizeof(ilabel), "deg=%d", ideg);
            sw_tan(&tan_old_int, rt, FR_TanI(ideg), ilabel);
        }

        total_new++;
    }

    /* Old path: FR_Tan takes s16, so use radix=6 (s9.6) to cover ±512 deg */
    long total_old = 0;
    for (long d = -32768; d <= 32767; d++) {
        s16 dval = (s16)d;
        double deg_dbl = (double)d / 64.0;
        double rad     = deg_dbl * M_PI / 180.0;
        char label[64];
        snprintf(label, sizeof(label), "d6=%d (%.3f deg)", (int)d, deg_dbl);

        sw_cos(&cos_old, cos(rad), FR_Cos(dval, 6), label);
        sw_tan(&tan_old_s16, tan(rad), FR_Tan(dval, 6), label);
        total_old++;
    }

    /* timing */
    {
        volatile s32 sink = 0;
        double t0, t1;
        long N = 65536;

        t0 = now_ns();
        for (long d = -32768; d <= 32767; d++) sink += FR_Cos((s16)d, 6);
        t1 = now_ns();
        cos_old.total_ns = t1 - t0; cos_old.time_count = N;

        t0 = now_ns();
        for (long d = -32768; d <= 32767; d++) sink += FR_Tan((s16)d, 6);
        t1 = now_ns();
        tan_old_s16.total_ns = t1 - t0; tan_old_s16.time_count = N;

        s32 tstep = (2 * deg360_s16) / N;
        t0 = now_ns();
        for (s32 d = -deg360_s16; d <= deg360_s16; d += tstep)
            sink += fr_tan_deg32(d, 16);
        t1 = now_ns();
        tan_new_full.total_ns = t1 - t0; tan_new_full.time_count = N;

        t0 = now_ns();
        for (long i = -360; i < 360; i++) sink += FR_TanI((s16)i);
        t1 = now_ns();
        tan_old_int.total_ns = t1 - t0; tan_old_int.time_count = 720;

        (void)sink;
    }

    printf("### Degree domain\n\n");
    printf("fr_tan_deg32: s32 input, radix=16, every 1/1024 deg, ±360 deg (%ld values)\n", total_new);
    printf("FR_Tan:       s16 input, radix=6 (s9.6), all 65536 s16 values (%ld values)\n\n", total_old);
    sw_header();
    sw_print(&cos_old);
    sw_print(&tan_old_s16);
    sw_print(&tan_new_full);
    sw_print(&tan_old_int);
    printf("\ntan samples: old_s16=%ld, new_s32=%ld, old_int=%ld\n\n",
           tan_old_s16.count, tan_new_full.count, tan_old_int.count);
}

/* ── main ──────────────────────────────────────────── */

int main(void)
{
    printf("FR_Math exhaustive error sweep: cos, tan (old), tan32 (new)\n");
    printf("============================================================\n");
    printf("cos: error = %% of full scale (1.0)\n");
    printf("tan: relative %% when |ref|>=0.01, absolute when near zero, skip |ref|>1000\n\n");

    sweep_bam();
    sweep_rad();
    sweep_deg();

    printf("Done.\n");
    return 0;
}
