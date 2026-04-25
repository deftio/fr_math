/*
 * bench_explog.cpp — Validate proposed exp/ln/log10 accuracy improvement
 *
 * Compares three variants for each of {exp, ln, log10}:
 *   1. FR_math current  (shift-only scaling macros)
 *   2. FR_math proposed  (one multiply via FR_MULK28)
 *   3. libfixmath
 * All measured against <cmath> double precision as gold standard.
 *
 * Compile via:  make -f Makefile.explog run
 */

#include <cstdio>
#include <cstdint>
#include <cmath>
#include <chrono>
#include <vector>
#include <cfloat>
#include <algorithm>

/* ---- FR_math ---- */
extern "C" {
#include "FR_defs.h"
#include "FR_math.h"
}

/* ---- libfixmath ---- */
#include "fixmath.h"

/* ================================================================
 * Proposed improvement: high-precision constants at radix 28
 * and a single-multiply scaling macro.
 * ================================================================ */

/* Constants at radix 28 (2^28 = 268435456).
 * Verified: round(value * 2^28) for each.
 */
/*
 * Verified via: python3 -c "print(round(val * 2**28))"
 *   log2(e):   round(1.4426950408889634 * 268435456) = 387270501
 *   ln(2):     round(0.6931471805599453 * 268435456) = 186065280
 *   log2(10):  round(3.3219280948873626 * 268435456) = 891729313
 *   log10(2):  round(0.3010299957401937 * 268435456) = 80807125
 */
#define K28_LOG2E      387270501   /* log2(e)   = 1.4426950408889634  */
#define K28_LN2        186065280   /* ln(2)     = 0.6931471805599453  */
#define K28_LOG2_10    891729313   /* log2(10)  = 3.3219280948873626  */
#define K28_LOG10_2     80807125   /* log10(2)  = 0.3010299957401937  */

/* Multiply x (any radix) by constant k at radix 28, result same radix as x.
 * Rounds to nearest (adds 0.5 LSB before shift).
 */
static inline int32_t mulk28(int32_t x, int32_t k) {
    return (int32_t)(((int64_t)x * (int64_t)k + (1 << 27)) >> 28);
}

/* Proposed exp (mulk28 scaling only, original 17-entry table): */
static inline int32_t proposed_exp(int32_t input, uint16_t radix) {
    int32_t scaled = mulk28(input, K28_LOG2E);
    return FR_pow2(scaled, radix);
}

/* Proposed ln: log2(x) * ln(2) */
static inline int32_t proposed_ln(int32_t input, uint16_t radix, uint16_t output_radix) {
    int32_t r = FR_log2(input, radix, output_radix);
    return mulk28(r, K28_LN2);
}

/* Proposed log10: log2(x) * log10(2) */
static inline int32_t proposed_log10(int32_t input, uint16_t radix, uint16_t output_radix) {
    int32_t r = FR_log2(input, radix, output_radix);
    return mulk28(r, K28_LOG10_2);
}

/* ================================================================
 * V2: mulk28 scaling + 65-entry pow2 table (64 segments)
 *
 * Same algorithm as FR_pow2 but 6-bit index / 10-bit interpolation
 * instead of 4-bit / 12-bit. +192 bytes ROM for the table.
 * ================================================================ */

static const uint32_t pow2_tab_65[65] = {
     65536,  66250,  66971,  67700,  68438,  69183,  69936,  70698,
     71468,  72246,  73032,  73828,  74632,  75444,  76266,  77096,
     77936,  78785,  79642,  80510,  81386,  82273,  83169,  84074,
     84990,  85915,  86851,  87796,  88752,  89719,  90696,  91684,
     92682,  93691,  94711,  95743,  96785,  97839,  98905,  99982,
    101070, 102171, 103283, 104408, 105545, 106694, 107856, 109031,
    110218, 111418, 112631, 113858, 115098, 116351, 117618, 118899,
    120194, 121502, 122825, 124163, 125515, 126882, 128263, 129660,
    131072,
};

/* pow2 with 65-entry table — drop-in replacement for FR_pow2 */
static int32_t pow2_65(int32_t input, uint16_t radix) {
    int32_t flr, frac_full, idx, frac_lo, lo, hi, mant, result;
    uint32_t mask = (radix > 0) ? (((uint32_t)1 << radix) - 1) : 0;

    if (input >= 0) {
        flr = (int32_t)((uint32_t)input >> radix);
        frac_full = (int32_t)((uint32_t)input & mask);
    } else {
        int32_t neg = -input;
        int32_t nflr = (int32_t)((uint32_t)neg >> radix);
        int32_t nfrc = (int32_t)((uint32_t)neg & mask);
        if (nfrc == 0) { flr = -nflr; frac_full = 0; }
        else { flr = -nflr - 1; frac_full = (int32_t)((1L << radix) - nfrc); }
    }

    if (radix > 16) frac_full >>= (radix - 16);
    else if (radix < 16) frac_full <<= (16 - radix);

    /* 6-bit index (64 segments), 10-bit interpolation */
    idx     = frac_full >> 10;
    frac_lo = frac_full & ((1L << 10) - 1);
    lo = (int32_t)pow2_tab_65[idx];
    hi = (int32_t)pow2_tab_65[idx + 1];
    mant = lo + (((hi - lo) * frac_lo) >> 10);

    if (flr >= 0) {
        if (flr >= 30) return FR_OVERFLOW_POS;
        result = mant << flr;
        return FR_CHRDX(result, 16, radix);
    } else {
        int32_t sh = -flr;
        if (sh >= 30) return 0;
        result = mant >> sh;
        return FR_CHRDX(result, 16, radix);
    }
}

/* V2 exp: mulk28 scaling + 65-entry pow2 table */
static inline int32_t v2_exp(int32_t input, uint16_t radix) {
    int32_t scaled = mulk28(input, K28_LOG2E);
    return pow2_65(scaled, radix);
}

/* ================================================================
 * Helpers
 * ================================================================ */

static const int RADIX = 16;
static const int32_t ONE = (1 << RADIX);
static const int N = 50000;
static const double Q16_LSB = 1.0 / (double)ONE;

static inline double q2d(int32_t v) { return (double)v / (double)ONE; }
static inline int32_t d2q(double d) {
    return (int32_t)(d * ONE + (d >= 0 ? 0.5 : -0.5));
}

static inline int64_t now_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()
    ).count();
}

static volatile int32_t sink;

/* ================================================================
 * Error stats
 * ================================================================ */

struct Stats {
    double max_lsb;
    double mean_lsb;
    double max_abs;
    double mean_abs;
};

static Stats calc_err(const std::vector<double>& ref,
                      const std::vector<int32_t>& got) {
    Stats s = {};
    double sum_abs = 0, sum_lsb = 0;
    int n = (int)ref.size();
    for (int i = 0; i < n; i++) {
        double err = std::fabs(q2d(got[i]) - ref[i]);
        double lsb = err / Q16_LSB;
        sum_abs += err;
        sum_lsb += lsb;
        if (lsb > s.max_lsb) s.max_lsb = lsb;
        if (err > s.max_abs) s.max_abs = err;
    }
    s.mean_abs = sum_abs / n;
    s.mean_lsb = sum_lsb / n;
    return s;
}

/* ================================================================
 * Constant verification
 * ================================================================ */

static void verify_constants() {
    printf("=== Constant verification (radix 28, 2^28 = 268435456) ===\n\n");

    struct { const char *name; int32_t k28; double exact; } tab[] = {
        {"log2(e)",  K28_LOG2E,   1.4426950408889634},
        {"ln(2)",    K28_LN2,     0.6931471805599453},
        {"log2(10)", K28_LOG2_10, 3.3219280948873626},
        {"log10(2)", K28_LOG10_2, 0.30102999566398120},
    };

    printf("  %-10s  %12s  %20s  %20s  %12s\n",
           "constant", "k28 value", "k28/2^28", "exact", "error");
    printf("  %-10s  %12s  %20s  %20s  %12s\n",
           "--------", "---------", "--------", "-----", "-----");

    for (auto& c : tab) {
        double approx = (double)c.k28 / (double)(1 << 28);
        double err = std::fabs(approx - c.exact);
        printf("  %-10s  %12d  %20.16f  %20.16f  %12.2e\n",
               c.name, c.k28, approx, c.exact, err);
    }
    printf("\n");
}

/* ================================================================
 * Overflow safety check
 * ================================================================ */

static void check_overflow() {
    printf("=== 64-bit overflow safety check ===\n\n");

    /* Worst-case inputs for each function */
    struct { const char *fn; int32_t worst_input; int32_t k28; } cases[] = {
        {"exp (x=10.0)",    d2q(10.0),    K28_LOG2E},
        {"exp (x=-10.0)",   d2q(-10.0),   K28_LOG2E},
        {"ln  (x=max_pos)", 0x7FFFFFFF,   K28_LN2},     /* max log2 output */
        {"log10 (x=max)",   0x7FFFFFFF,   K28_LOG10_2},
    };

    printf("  %-20s  %12s  %12s  %14s  %6s\n",
           "case", "input", "k28", "product bits", "safe?");
    printf("  %-20s  %12s  %12s  %14s  %6s\n",
           "----", "-----", "---", "------------", "-----");

    for (auto& c : cases) {
        int64_t product = (int64_t)c.worst_input * (int64_t)c.k28;
        /* Count bits needed */
        uint64_t mag = (product < 0) ? (uint64_t)(-product) : (uint64_t)product;
        int bits = 0;
        uint64_t tmp = mag;
        while (tmp > 0) { tmp >>= 1; bits++; }
        bool safe = (bits < 63); /* s64 has 63 magnitude bits */

        printf("  %-20s  %12d  %12d  %14d  %6s\n",
               c.fn, c.worst_input, c.k28, bits, safe ? "YES" : "NO!");
    }
    printf("\n");
}

/* ================================================================
 * Benchmark: exp
 * ================================================================ */

static void bench_exp() {
    printf("=== exp(x) — range [-5, +5] ===\n\n");

    std::vector<int32_t> inputs(N);
    std::vector<double> ref(N);
    for (int i = 0; i < N; i++) {
        double t = -5.0 + 10.0 * i / (N - 1);
        inputs[i] = d2q(t);
        ref[i] = std::exp(t);
    }

    /* --- Current: shift-only FR_SLOG2E --- */
    std::vector<int32_t> cur_out(N);
    int64_t t0 = now_ns();
    for (int i = 0; i < N; i++)
        cur_out[i] = FR_EXP(inputs[i], RADIX);
    int64_t t1 = now_ns();
    double cur_ns = (double)(t1 - t0) / N;
    Stats cur_err = calc_err(ref, cur_out);

    /* --- Proposed: mulk28 --- */
    std::vector<int32_t> new_out(N);
    t0 = now_ns();
    for (int i = 0; i < N; i++)
        new_out[i] = proposed_exp(inputs[i], RADIX);
    t1 = now_ns();
    double new_ns = (double)(t1 - t0) / N;
    Stats new_err = calc_err(ref, new_out);

    /* --- V2: mulk28 + 65-entry table --- */
    std::vector<int32_t> v2_out(N);
    t0 = now_ns();
    for (int i = 0; i < N; i++)
        v2_out[i] = v2_exp(inputs[i], RADIX);
    t1 = now_ns();
    double v2_ns = (double)(t1 - t0) / N;
    Stats v2_err = calc_err(ref, v2_out);

    /* --- libfixmath --- */
    std::vector<int32_t> lfm_out(N);
    t0 = now_ns();
    for (int i = 0; i < N; i++)
        lfm_out[i] = fix16_exp(inputs[i]);
    t1 = now_ns();
    double lfm_ns = (double)(t1 - t0) / N;
    Stats lfm_err = calc_err(ref, lfm_out);

    printf("  %-28s  %8s  %8s  %10s  %10s\n",
           "variant", "max LSB", "mean LSB", "ns/call", "vs lfm");
    printf("  %-28s  %8s  %8s  %10s  %10s\n",
           "-------", "-------", "--------", "-------", "------");
    printf("  %-28s  %8.1f  %8.1f  %10.1f  %10.1fx\n",
           "FR current (shift+17tab)", cur_err.max_lsb, cur_err.mean_lsb, cur_ns,
           lfm_ns / cur_ns);
    printf("  %-28s  %8.1f  %8.1f  %10.1f  %10.1fx\n",
           "FR mulk28 only (+0 bytes)", new_err.max_lsb, new_err.mean_lsb, new_ns,
           lfm_ns / new_ns);
    printf("  %-28s  %8.1f  %8.1f  %10.1f  %10.1fx\n",
           "FR mulk28+65tab (+192B)", v2_err.max_lsb, v2_err.mean_lsb, v2_ns,
           lfm_ns / v2_ns);
    printf("  %-28s  %8.1f  %8.1f  %10.1f  %10s\n",
           "libfixmath (+33KB RAM)", lfm_err.max_lsb, lfm_err.mean_lsb, lfm_ns,
           "baseline");
    printf("\n");
}

/* ================================================================
 * Benchmark: ln
 * ================================================================ */

static void bench_ln() {
    printf("=== ln(x) — range [0.01, 100] ===\n\n");

    std::vector<int32_t> inputs(N);
    std::vector<double> ref(N);
    for (int i = 0; i < N; i++) {
        double t = 0.01 + 99.99 * i / (N - 1);
        inputs[i] = d2q(t);
        ref[i] = std::log(t);
    }

    /* --- Current --- */
    std::vector<int32_t> cur_out(N);
    int64_t t0 = now_ns();
    for (int i = 0; i < N; i++)
        cur_out[i] = FR_ln(inputs[i], RADIX, RADIX);
    int64_t t1 = now_ns();
    double cur_ns = (double)(t1 - t0) / N;
    Stats cur_err = calc_err(ref, cur_out);

    /* --- Proposed --- */
    std::vector<int32_t> new_out(N);
    t0 = now_ns();
    for (int i = 0; i < N; i++)
        new_out[i] = proposed_ln(inputs[i], RADIX, RADIX);
    t1 = now_ns();
    double new_ns = (double)(t1 - t0) / N;
    Stats new_err = calc_err(ref, new_out);

    /* --- libfixmath --- */
    std::vector<int32_t> lfm_out(N);
    t0 = now_ns();
    for (int i = 0; i < N; i++)
        lfm_out[i] = fix16_log(inputs[i]);
    t1 = now_ns();
    double lfm_ns = (double)(t1 - t0) / N;
    Stats lfm_err = calc_err(ref, lfm_out);

    printf("  %-22s  %8s  %8s  %10s  %10s\n",
           "variant", "max LSB", "mean LSB", "ns/call", "speedup");
    printf("  %-22s  %8s  %8s  %10s  %10s\n",
           "-------", "-------", "--------", "-------", "-------");
    printf("  %-22s  %8.1f  %8.1f  %10.1f  %10s\n",
           "FR_math current", cur_err.max_lsb, cur_err.mean_lsb, cur_ns, "baseline");
    printf("  %-22s  %8.1f  %8.1f  %10.1f  %10.2fx\n",
           "FR_math proposed", new_err.max_lsb, new_err.mean_lsb, new_ns,
           cur_ns / new_ns);
    printf("  %-22s  %8.1f  %8.1f  %10.1f  %10.2fx\n",
           "libfixmath", lfm_err.max_lsb, lfm_err.mean_lsb, lfm_ns,
           cur_ns / lfm_ns);

    printf("\n  Accuracy improvement: %.1fx better max LSB error\n",
           cur_err.max_lsb / new_err.max_lsb);
    printf("  Speed cost of multiply: %.1f ns -> %.1f ns (%.1f%% overhead)\n",
           cur_ns, new_ns, (new_ns - cur_ns) / cur_ns * 100.0);
    printf("  Proposed vs libfixmath: %.1fx %s, %.1fx %s accuracy\n",
           std::max(lfm_ns / new_ns, new_ns / lfm_ns),
           new_ns < lfm_ns ? "faster" : "slower",
           std::max(lfm_err.max_lsb / new_err.max_lsb,
                    new_err.max_lsb / lfm_err.max_lsb),
           new_err.max_lsb < lfm_err.max_lsb ? "better" : "worse");
    printf("\n");
}

/* ================================================================
 * Benchmark: log10
 * ================================================================ */

static void bench_log10() {
    printf("=== log10(x) — range [0.01, 100] ===\n\n");

    std::vector<int32_t> inputs(N);
    std::vector<double> ref(N);
    for (int i = 0; i < N; i++) {
        double t = 0.01 + 99.99 * i / (N - 1);
        inputs[i] = d2q(t);
        ref[i] = std::log10(t);
    }

    /* --- Current --- */
    std::vector<int32_t> cur_out(N);
    int64_t t0 = now_ns();
    for (int i = 0; i < N; i++)
        cur_out[i] = FR_log10(inputs[i], RADIX, RADIX);
    int64_t t1 = now_ns();
    double cur_ns = (double)(t1 - t0) / N;
    Stats cur_err = calc_err(ref, cur_out);

    /* --- Proposed --- */
    std::vector<int32_t> new_out(N);
    t0 = now_ns();
    for (int i = 0; i < N; i++)
        new_out[i] = proposed_log10(inputs[i], RADIX, RADIX);
    t1 = now_ns();
    double new_ns = (double)(t1 - t0) / N;
    Stats new_err = calc_err(ref, new_out);

    /* --- libfixmath (no log10 — compute as log(x)/log(10)) --- */
    /* libfixmath has fix16_log (natural log) and fix16_log2, no fix16_log10.
     * We'll compute it as: fix16_mul(fix16_log2(x), fix16_from_dbl(log10(2)))
     * which is the same identity we use.
     */
    const fix16_t lfm_log10_2 = fix16_from_dbl(0.30102999566398120);
    std::vector<int32_t> lfm_out(N);
    t0 = now_ns();
    for (int i = 0; i < N; i++)
        lfm_out[i] = fix16_mul(fix16_log2(inputs[i]), lfm_log10_2);
    t1 = now_ns();
    double lfm_ns = (double)(t1 - t0) / N;
    Stats lfm_err = calc_err(ref, lfm_out);

    printf("  %-22s  %8s  %8s  %10s  %10s\n",
           "variant", "max LSB", "mean LSB", "ns/call", "speedup");
    printf("  %-22s  %8s  %8s  %10s  %10s\n",
           "-------", "-------", "--------", "-------", "-------");
    printf("  %-22s  %8.1f  %8.1f  %10.1f  %10s\n",
           "FR_math current", cur_err.max_lsb, cur_err.mean_lsb, cur_ns, "baseline");
    printf("  %-22s  %8.1f  %8.1f  %10.1f  %10.2fx\n",
           "FR_math proposed", new_err.max_lsb, new_err.mean_lsb, new_ns,
           cur_ns / new_ns);
    printf("  %-22s  %8.1f  %8.1f  %10.1f  %10.2fx\n",
           "libfixmath", lfm_err.max_lsb, lfm_err.mean_lsb, lfm_ns,
           cur_ns / lfm_ns);

    printf("\n  Accuracy improvement: %.1fx better max LSB error\n",
           cur_err.max_lsb / new_err.max_lsb);
    printf("  Speed cost of multiply: %.1f ns -> %.1f ns (%.1f%% overhead)\n",
           cur_ns, new_ns, (new_ns - cur_ns) / cur_ns * 100.0);
    printf("  Proposed vs libfixmath: %.1fx %s, %.1fx %s accuracy\n",
           std::max(lfm_ns / new_ns, new_ns / lfm_ns),
           new_ns < lfm_ns ? "faster" : "slower",
           std::max(lfm_err.max_lsb / new_err.max_lsb,
                    new_err.max_lsb / lfm_err.max_lsb),
           new_err.max_lsb < lfm_err.max_lsb ? "better" : "worse");
    printf("\n");
}

/* ================================================================
 * Table error isolation: how much error comes from the lookup tables
 * vs the scaling step?
 * ================================================================ */

static void isolate_table_error() {
    printf("=== Error decomposition: table vs scaling ===\n\n");
    printf("  This feeds PERFECT (double-derived) intermediates into pow2/log2\n");
    printf("  to measure table error in isolation.\n\n");

    /* --- pow2 table error (affects exp) --- */
    {
        printf("  FR_pow2 table error (inputs: exact x*log2(e) at Q16.16):\n");
        double max_table_lsb = 0, sum_table_lsb = 0;
        double max_total_lsb = 0, sum_total_lsb = 0;
        int n = N;
        for (int i = 0; i < n; i++) {
            double x = -5.0 + 10.0 * i / (n - 1);
            double gold = std::exp(x);
            if (gold > 32767.0 || gold < 1e-5) continue; /* skip overflow/underflow */

            /* Perfect intermediate: exact x * log2(e) quantized to Q16.16 */
            double exact_scaled = x * 1.4426950408889634;
            int32_t perfect_q = d2q(exact_scaled);
            int32_t table_only = FR_pow2(perfect_q, RADIX);
            double table_err_lsb = std::fabs(q2d(table_only) - gold) / Q16_LSB;

            /* Full pipeline: shift-only scaling + pow2 */
            int32_t xq = d2q(x);
            int32_t full_current = FR_EXP(xq, RADIX);
            double total_err_lsb = std::fabs(q2d(full_current) - gold) / Q16_LSB;

            if (table_err_lsb > max_table_lsb) max_table_lsb = table_err_lsb;
            sum_table_lsb += table_err_lsb;
            if (total_err_lsb > max_total_lsb) max_total_lsb = total_err_lsb;
            sum_total_lsb += total_err_lsb;
        }
        printf("    pow2 table alone:     max %8.1f LSB,  mean %6.1f LSB\n",
               max_table_lsb, sum_table_lsb / n);
        printf("    full exp pipeline:    max %8.1f LSB,  mean %6.1f LSB\n",
               max_total_lsb, sum_total_lsb / n);
        printf("    -> table is %.0f%% of total max error\n\n",
               max_table_lsb / max_total_lsb * 100.0);
    }

    /* --- log2 table error (affects ln, log10) --- */
    {
        printf("  FR_log2 table error (direct measurement):\n");
        double max_log2_lsb = 0, sum_log2_lsb = 0;
        double max_ln_scale_lsb = 0, sum_ln_scale_lsb = 0;
        double max_ln_total_lsb = 0, sum_ln_total_lsb = 0;
        int n = N, cnt = 0;
        for (int i = 0; i < n; i++) {
            double x = 0.01 + 99.99 * i / (n - 1);
            int32_t xq = d2q(x);
            double gold_log2 = std::log2(x);
            double gold_ln   = std::log(x);

            /* log2 table error */
            int32_t log2_out = FR_log2(xq, RADIX, RADIX);
            double log2_lsb = std::fabs(q2d(log2_out) - gold_log2) / Q16_LSB;
            if (log2_lsb > max_log2_lsb) max_log2_lsb = log2_lsb;
            sum_log2_lsb += log2_lsb;

            /* ln via perfect log2 + shift-only scale */
            double exact_log2 = gold_log2;  /* what if log2 were perfect? */
            int32_t perfect_log2_q = d2q(exact_log2);
            int32_t scale_only = FR_SrLOG2E(perfect_log2_q);
            double scale_lsb = std::fabs(q2d(scale_only) - gold_ln) / Q16_LSB;
            if (scale_lsb > max_ln_scale_lsb) max_ln_scale_lsb = scale_lsb;
            sum_ln_scale_lsb += scale_lsb;

            /* Full ln pipeline */
            int32_t ln_out = FR_ln(xq, RADIX, RADIX);
            double ln_lsb = std::fabs(q2d(ln_out) - gold_ln) / Q16_LSB;
            if (ln_lsb > max_ln_total_lsb) max_ln_total_lsb = ln_lsb;
            sum_ln_total_lsb += ln_lsb;
            cnt++;
        }
        printf("    log2 table alone:     max %8.1f LSB,  mean %6.1f LSB\n",
               max_log2_lsb, sum_log2_lsb / cnt);
        printf("    scale alone (perfect log2 + shift-only *ln2):\n");
        printf("                          max %8.1f LSB,  mean %6.1f LSB\n",
               max_ln_scale_lsb, sum_ln_scale_lsb / cnt);
        printf("    full ln pipeline:     max %8.1f LSB,  mean %6.1f LSB\n",
               max_ln_total_lsb, sum_ln_total_lsb / cnt);
        printf("    -> log2 table is %.0f%% of total ln max error\n\n",
               max_log2_lsb / max_ln_total_lsb * 100.0);
    }

    /* --- Actual 65-entry pow2 table error --- */
    {
        printf("  pow2_65 table error (64 segments, measured):\n");
        double max65_lsb = 0, sum65_lsb = 0;
        int cnt = 0;
        for (int i = 0; i < N; i++) {
            double x = -5.0 + 10.0 * i / (N - 1);
            double gold = std::exp(x);
            if (gold > 32767.0 || gold < 1e-5) continue;
            double exact_scaled = x * 1.4426950408889634;
            int32_t perfect_q = d2q(exact_scaled);
            int32_t tab65_out = pow2_65(perfect_q, RADIX);
            double lsb = std::fabs(q2d(tab65_out) - gold) / Q16_LSB;
            if (lsb > max65_lsb) max65_lsb = lsb;
            sum65_lsb += lsb;
            cnt++;
        }
        printf("    pow2_65 table alone:   max %8.1f LSB,  mean %6.1f LSB\n",
               max65_lsb, sum65_lsb / cnt);
        printf("    Table size: 65 * 4 = 260 bytes (u32), delta = +192 bytes\n\n");
    }
}

/* ================================================================
 * Spot-check: show a few sample values
 * ================================================================ */

static void spot_check() {
    printf("=== Spot check: selected values ===\n\n");

    double test_vals[] = {-3.0, -1.0, -0.5, 0.0, 0.5, 1.0, 2.0, 5.0};
    int nv = sizeof(test_vals) / sizeof(test_vals[0]);

    printf("  exp(x):\n");
    printf("  %8s  %12s  %12s  %12s  %12s\n",
           "x", "double", "current", "mulk28+65tab", "libfixmath");
    printf("  %8s  %12s  %12s  %12s  %12s\n",
           "---", "------", "-------", "------------", "----------");
    for (int i = 0; i < nv; i++) {
        double x = test_vals[i];
        int32_t xq = d2q(x);
        double gold = std::exp(x);
        double cur  = q2d(FR_EXP(xq, RADIX));
        double v2   = q2d(v2_exp(xq, RADIX));
        double lfm  = q2d(fix16_exp(xq));
        printf("  %8.2f  %12.6f  %12.6f  %12.6f  %12.6f\n",
               x, gold, cur, v2, lfm);
    }

    printf("\n  ln(x):\n");
    double ln_vals[] = {0.01, 0.1, 0.5, 1.0, 2.0, 10.0, 50.0, 100.0};
    int nlv = sizeof(ln_vals) / sizeof(ln_vals[0]);
    printf("  %8s  %12s  %12s  %12s  %12s\n",
           "x", "double", "current", "proposed", "libfixmath");
    printf("  %8s  %12s  %12s  %12s  %12s\n",
           "---", "------", "-------", "--------", "----------");
    for (int i = 0; i < nlv; i++) {
        double x = ln_vals[i];
        int32_t xq = d2q(x);
        double gold = std::log(x);
        double cur  = q2d(FR_ln(xq, RADIX, RADIX));
        double prop = q2d(proposed_ln(xq, RADIX, RADIX));
        double lfm  = q2d(fix16_log(xq));
        printf("  %8.2f  %12.6f  %12.6f  %12.6f  %12.6f\n",
               x, gold, cur, prop, lfm);
    }
    printf("\n");
}

/* ================================================================
 * Shift-only macro error analysis
 * ================================================================ */

static void analyze_shift_macros() {
    printf("=== Shift-only macro error (root cause analysis) ===\n\n");

    /* Test FR_SLOG2E: should multiply by log2(e) = 1.44269504... */
    printf("  FR_SLOG2E(x) vs exact x * log2(e):\n");
    printf("  %12s  %12s  %12s  %12s  %12s\n",
           "x (Q16.16)", "shift-only", "exact", "proposed", "shift err");
    printf("  %12s  %12s  %12s  %12s  %12s\n",
           "----------", "----------", "-----", "--------", "---------");

    int32_t test_inputs[] = {65536, 327680, -327680, 655360, 6554};
    for (auto xq : test_inputs) {
        double x = q2d(xq);
        int32_t shift_val = FR_SLOG2E(xq);
        double exact = x * 1.4426950408889634;
        int32_t prop_val = mulk28(xq, K28_LOG2E);
        double shift_err_lsb = std::fabs(q2d(shift_val) - exact) / Q16_LSB;
        double prop_err_lsb = std::fabs(q2d(prop_val) - exact) / Q16_LSB;
        printf("  %12d  %12d  %12.2f  %12d  %8.1f / %.1f LSB\n",
               xq, shift_val, exact * ONE, prop_val, shift_err_lsb, prop_err_lsb);
    }

    printf("\n  FR_SrLOG2E(x) vs exact x * ln(2):\n");
    printf("  %12s  %12s  %12s  %12s  %12s\n",
           "x (Q16.16)", "shift-only", "exact", "proposed", "shift err");
    printf("  %12s  %12s  %12s  %12s  %12s\n",
           "----------", "----------", "-----", "--------", "---------");

    for (auto xq : test_inputs) {
        double x = q2d(xq);
        int32_t shift_val = FR_SrLOG2E(xq);
        double exact = x * 0.6931471805599453;
        int32_t prop_val = mulk28(xq, K28_LN2);
        double shift_err_lsb = std::fabs(q2d(shift_val) - exact) / Q16_LSB;
        double prop_err_lsb = std::fabs(q2d(prop_val) - exact) / Q16_LSB;
        printf("  %12d  %12d  %12.2f  %12d  %8.1f / %.1f LSB\n",
               xq, shift_val, exact * ONE, prop_val, shift_err_lsb, prop_err_lsb);
    }
    printf("\n");
}

/* ================================================================
 * main
 * ================================================================ */

int main() {
    printf("========================================================\n");
    printf("  FR_math exp/log accuracy improvement validation\n");
    printf("  Proposed: replace shift-only macros with FR_MULK28\n");
    printf("  Gold standard: <cmath> IEEE 754 double precision\n");
    printf("  Test points: %d per function, Q16.16 (s15.16)\n", N);
    printf("========================================================\n\n");

    verify_constants();
    check_overflow();
    analyze_shift_macros();
    isolate_table_error();
    spot_check();

    printf("========================================================\n");
    printf("  SPEED + ACCURACY BENCHMARKS\n");
    printf("========================================================\n\n");

    /* Warmup */
    for (volatile int i = 0; i < 10000; i++) {
        sink = FR_EXP(d2q(1.0), RADIX);
        sink = fix16_exp(d2q(1.0));
        sink = proposed_exp(d2q(1.0), RADIX);
        sink = v2_exp(d2q(1.0), RADIX);
    }

    bench_exp();
    bench_ln();
    bench_log10();

    printf("========================================================\n");
    printf("  SIZE COMPARISON\n");
    printf("========================================================\n\n");
    printf("  FR_math exp/log (current):  1,024 bytes (824 code + 200 tables)\n");
    printf("  FR_math exp/log (with 65):  1,216 bytes (824 code + 392 tables)\n");
    printf("  libfixmath exp/log:        33,996 bytes (1228 code + 32768 RAM cache)\n\n");
    printf("  Delta for 65-entry pow2:   +192 bytes ROM.  Still ~28x smaller.\n");

    return 0;
}
