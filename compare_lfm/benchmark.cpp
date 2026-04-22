/*
 * benchmark.cpp — FR_math vs libfixmath comparison
 *
 * WARNING: This file lives in .compare/ and must NOT touch any
 *          files outside this directory.
 *
 * Both libraries use Q16.16 (s15.16) fixed-point: 1.0 = 65536.
 * We compare: speed (wall-clock ns per call) and accuracy (vs double).
 *
 * Output:
 *   stdout → JSON  (machine-readable)
 *   stderr → markdown summary table (human-readable)
 *
 * Compile via the accompanying Makefile.
 */

#include <cstdio>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <chrono>
#include <vector>
#include <string>
#include <algorithm>
#include <cfloat>

/* ---- FR_math ---- */
extern "C" {
#include "FR_defs.h"
#include "FR_math.h"
}

/* ---- libfixmath ---- */
#include "fixmath.h"

/* ================================================================
 * Helpers
 * ================================================================ */

static const int RADIX = 16;                    /* s15.16 */
static const int32_t ONE = (1 << RADIX);        /* 65536  */

/* Separate counts for accuracy (many points) and timing (tight loop) */
static const int N_ACCURACY = 65536;
static const int N_TIMING   = 100000;

/* Near-zero threshold for relative error — skip |ref| < 0.01 to avoid
 * misleading spikes (1/0.01 = 100x max amplification). Matches the
 * threshold used in the TDD characterization suite. */
static const double REL_THRESH = 0.01;

/* Convert Q16.16 to double */
static inline double q16_to_dbl(int32_t v) {
    return (double)v / (double)ONE;
}

/* Convert double to Q16.16 */
static inline int32_t dbl_to_q16(double d) {
    return (int32_t)(d * ONE + (d >= 0 ? 0.5 : -0.5));
}

/* High-resolution timer returning nanoseconds */
static inline int64_t now_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()
    ).count();
}

/* ================================================================
 * Test-point generators (in Q16.16)
 * ================================================================ */

/* Angle inputs in radians Q16.16: sweep -pi to +pi */
static std::vector<int32_t> make_angle_inputs(int n) {
    std::vector<int32_t> v(n);
    double lo = -M_PI, hi = M_PI;
    for (int i = 0; i < n; i++) {
        double t = lo + (hi - lo) * i / (n - 1);
        v[i] = dbl_to_q16(t);
    }
    return v;
}

/* Inputs in [-0.999, +0.999] Q16.16 for asin/acos */
static std::vector<int32_t> make_unit_inputs(int n) {
    std::vector<int32_t> v(n);
    double lo = -0.999, hi = 0.999;
    for (int i = 0; i < n; i++) {
        double t = lo + (hi - lo) * i / (n - 1);
        v[i] = dbl_to_q16(t);
    }
    return v;
}

/* Positive inputs (0.01 .. 100) for sqrt / log */
static std::vector<int32_t> make_pos_inputs(int n) {
    std::vector<int32_t> v(n);
    double lo = 0.01, hi = 100.0;
    for (int i = 0; i < n; i++) {
        double t = lo + (hi - lo) * i / (n - 1);
        v[i] = dbl_to_q16(t);
    }
    return v;
}

/* General multiply/divide inputs: moderate range to avoid overflow */
static std::vector<int32_t> make_arith_inputs(int n) {
    std::vector<int32_t> v(n);
    double lo = -50.0, hi = 50.0;
    for (int i = 0; i < n; i++) {
        double t = lo + (hi - lo) * i / (n - 1);
        v[i] = dbl_to_q16(t);
    }
    return v;
}

/* exp inputs: small range to avoid overflow (e^x grows fast) */
static std::vector<int32_t> make_exp_inputs(int n) {
    std::vector<int32_t> v(n);
    double lo = -5.0, hi = 5.0;
    for (int i = 0; i < n; i++) {
        double t = lo + (hi - lo) * i / (n - 1);
        v[i] = dbl_to_q16(t);
    }
    return v;
}

/* atan2 inputs: 360° sweep at multiple radii, all 4 quadrants.
 * Returns parallel (x, y) vectors of length n. */
static void make_atan2_inputs(int n, std::vector<int32_t>& x_out,
                              std::vector<int32_t>& y_out) {
    x_out.resize(n);
    y_out.resize(n);
    /* 5 radii, n/5 angles each */
    double radii[] = {0.1, 1.0, 10.0, 100.0, 1000.0};
    int nrad = 5;
    int per_r = n / nrad;
    int idx = 0;
    for (int ri = 0; ri < nrad; ri++) {
        double r = radii[ri];
        for (int ai = 0; ai < per_r && idx < n; ai++, idx++) {
            double angle = -M_PI + 2.0 * M_PI * ai / per_r;
            x_out[idx] = dbl_to_q16(r * std::cos(angle));
            y_out[idx] = dbl_to_q16(r * std::sin(angle));
        }
    }
    /* fill remaining */
    for (; idx < n; idx++) {
        x_out[idx] = dbl_to_q16(1.0);
        y_out[idx] = dbl_to_q16(1.0);
    }
}

/* ================================================================
 * Error measurement
 * ================================================================ */

static const double Q16_LSB = 1.0 / (double)ONE;  /* 1.52587890625e-5 */

struct ErrorStats {
    double max_abs_err;    /* max |fixed - double_ref| in real units */
    double mean_abs_err;
    double max_lsb_err;    /* max error expressed in Q16.16 LSBs */
    double mean_lsb_err;
    double max_rel_err;    /* max |fixed - double_ref| / |double_ref| (%) */
    double mean_rel_err;   /* skips |ref| < REL_THRESH to avoid inf */
    int    count;
    int    rel_count;      /* how many points contributed to rel_err */
};

static ErrorStats compute_errors(const std::vector<double>& ref,
                                 const std::vector<int32_t>& fixed) {
    ErrorStats s = {};
    double sum_abs = 0, sum_lsb = 0, sum_rel = 0;
    int rel_count = 0;
    int n = (int)ref.size();
    for (int i = 0; i < n; i++) {
        double got = q16_to_dbl(fixed[i]);
        double err = std::fabs(got - ref[i]);
        double lsb = err / Q16_LSB;
        sum_abs += err;
        sum_lsb += lsb;
        if (err > s.max_abs_err) s.max_abs_err = err;
        if (lsb > s.max_lsb_err) s.max_lsb_err = lsb;
        if (std::fabs(ref[i]) > REL_THRESH) {
            double rel = err / std::fabs(ref[i]) * 100.0;
            sum_rel += rel;
            if (rel > s.max_rel_err) s.max_rel_err = rel;
            rel_count++;
        }
    }
    s.count = n;
    s.rel_count = rel_count;
    s.mean_abs_err = sum_abs / n;
    s.mean_lsb_err = sum_lsb / n;
    s.mean_rel_err = rel_count > 0 ? sum_rel / rel_count : 0;
    return s;
}

/* ================================================================
 * Per-function benchmark runner
 * ================================================================ */

struct BenchResult {
    std::string name;
    std::string gold_ref;          /* math.h function used as gold standard */
    double fr_ns_per_call;
    double lfm_ns_per_call;        /* -1 if libfixmath has no equivalent */
    ErrorStats fr_err;
    ErrorStats lfm_err;
    std::string note = {};         /* optional per-function note */
    std::string sweep = {};        /* sweep description for table */
};

/* Prevent compiler from optimizing away the result */
static volatile int32_t sink;

/* ================================================================
 * Timing helper — runs a function N_TIMING times, returns ns/call.
 * Uses 3 passes and takes the minimum to reduce variance.
 * ================================================================ */
template<typename Fn>
static double time_fn(Fn fn) {
    /* warm-up */
    for (int i = 0; i < 1000; i++) sink = fn();

    double best = 1e18;
    for (int pass = 0; pass < 3; pass++) {
        int64_t t0 = now_ns();
        for (int i = 0; i < N_TIMING; i++)
            sink = fn();
        int64_t t1 = now_ns();
        double ns = (double)(t1 - t0) / N_TIMING;
        if (ns < best) best = ns;
    }
    return best;
}

/* ================================================================
 * Individual benchmarks
 * ================================================================ */

static BenchResult bench_sin() {
    auto inputs = make_angle_inputs(N_ACCURACY);
    int n = N_ACCURACY;

    std::vector<double> ref(n);
    for (int i = 0; i < n; i++)
        ref[i] = std::sin(q16_to_dbl(inputs[i]));

    std::vector<int32_t> fr_out(n), lfm_out(n);
    for (int i = 0; i < n; i++) fr_out[i] = fr_sin(inputs[i], RADIX);
    for (int i = 0; i < n; i++) lfm_out[i] = fix16_sin(inputs[i]);

    int ti = 0;
    double fr_ns  = time_fn([&]{ return fr_sin(inputs[ti++ % n], RADIX); });
    ti = 0;
    double lfm_ns = time_fn([&]{ return fix16_sin(inputs[ti++ % n]); });

    return {"sin", "std::sin", fr_ns, lfm_ns,
            compute_errors(ref, fr_out), compute_errors(ref, lfm_out),
            {}, "65536-pt, [-pi, +pi]"};
}

static BenchResult bench_cos() {
    auto inputs = make_angle_inputs(N_ACCURACY);
    int n = N_ACCURACY;

    std::vector<double> ref(n);
    for (int i = 0; i < n; i++)
        ref[i] = std::cos(q16_to_dbl(inputs[i]));

    std::vector<int32_t> fr_out(n), lfm_out(n);
    for (int i = 0; i < n; i++) fr_out[i] = fr_cos(inputs[i], RADIX);
    for (int i = 0; i < n; i++) lfm_out[i] = fix16_cos(inputs[i]);

    int ti = 0;
    double fr_ns  = time_fn([&]{ return fr_cos(inputs[ti++ % n], RADIX); });
    ti = 0;
    double lfm_ns = time_fn([&]{ return fix16_cos(inputs[ti++ % n]); });

    return {"cos", "std::cos", fr_ns, lfm_ns,
            compute_errors(ref, fr_out), compute_errors(ref, lfm_out),
            {}, "65536-pt, [-pi, +pi]"};
}

static BenchResult bench_tan() {
    /* Avoid near ±pi/2 where tan explodes */
    int n = N_ACCURACY;
    std::vector<int32_t> inputs(n);
    double lo = -1.2, hi = 1.2;
    for (int i = 0; i < n; i++)
        inputs[i] = dbl_to_q16(lo + (hi - lo) * i / (n - 1));

    std::vector<double> ref(n);
    for (int i = 0; i < n; i++)
        ref[i] = std::tan(q16_to_dbl(inputs[i]));

    std::vector<int32_t> fr_out(n), lfm_out(n);
    for (int i = 0; i < n; i++) fr_out[i] = fr_tan(inputs[i], RADIX);
    for (int i = 0; i < n; i++) lfm_out[i] = fix16_tan(inputs[i]);

    int ti = 0;
    double fr_ns  = time_fn([&]{ return fr_tan(inputs[ti++ % n], RADIX); });
    ti = 0;
    double lfm_ns = time_fn([&]{ return fix16_tan(inputs[ti++ % n]); });

    return {"tan", "std::tan", fr_ns, lfm_ns,
            compute_errors(ref, fr_out), compute_errors(ref, lfm_out),
            "Skip near pi/2", "65536-pt, [-1.2, 1.2] rad"};
}

static BenchResult bench_asin() {
    auto inputs = make_unit_inputs(N_ACCURACY);
    int n = N_ACCURACY;

    std::vector<double> ref(n);
    for (int i = 0; i < n; i++)
        ref[i] = std::asin(q16_to_dbl(inputs[i]));

    std::vector<int32_t> fr_out(n), lfm_out(n);
    for (int i = 0; i < n; i++) fr_out[i] = FR_asin(inputs[i], RADIX, RADIX);
    for (int i = 0; i < n; i++) lfm_out[i] = fix16_asin(inputs[i]);

    int ti = 0;
    double fr_ns  = time_fn([&]{ return FR_asin(inputs[ti++ % n], RADIX, RADIX); });
    ti = 0;
    double lfm_ns = time_fn([&]{ return fix16_asin(inputs[ti++ % n]); });

    return {"asin", "std::asin", fr_ns, lfm_ns,
            compute_errors(ref, fr_out), compute_errors(ref, lfm_out),
            {}, "65536-pt, [-0.999, 0.999]"};
}

static BenchResult bench_acos() {
    auto inputs = make_unit_inputs(N_ACCURACY);
    int n = N_ACCURACY;

    std::vector<double> ref(n);
    for (int i = 0; i < n; i++)
        ref[i] = std::acos(q16_to_dbl(inputs[i]));

    std::vector<int32_t> fr_out(n), lfm_out(n);
    for (int i = 0; i < n; i++) fr_out[i] = FR_acos(inputs[i], RADIX, RADIX);
    for (int i = 0; i < n; i++) lfm_out[i] = fix16_acos(inputs[i]);

    int ti = 0;
    double fr_ns  = time_fn([&]{ return FR_acos(inputs[ti++ % n], RADIX, RADIX); });
    ti = 0;
    double lfm_ns = time_fn([&]{ return fix16_acos(inputs[ti++ % n]); });

    return {"acos", "std::acos", fr_ns, lfm_ns,
            compute_errors(ref, fr_out), compute_errors(ref, lfm_out),
            {}, "65536-pt, [-0.999, 0.999]"};
}

static BenchResult bench_atan() {
    auto inputs = make_arith_inputs(N_ACCURACY);
    int n = N_ACCURACY;

    std::vector<double> ref(n);
    for (int i = 0; i < n; i++)
        ref[i] = std::atan(q16_to_dbl(inputs[i]));

    std::vector<int32_t> fr_out(n), lfm_out(n);
    for (int i = 0; i < n; i++) fr_out[i] = FR_atan(inputs[i], RADIX, RADIX);
    for (int i = 0; i < n; i++) lfm_out[i] = fix16_atan(inputs[i]);

    int ti = 0;
    double fr_ns  = time_fn([&]{ return FR_atan(inputs[ti++ % n], RADIX, RADIX); });
    ti = 0;
    double lfm_ns = time_fn([&]{ return fix16_atan(inputs[ti++ % n]); });

    return {"atan", "std::atan", fr_ns, lfm_ns,
            compute_errors(ref, fr_out), compute_errors(ref, lfm_out),
            {}, "65536-pt, [-50, 50]"};
}

static BenchResult bench_atan2() {
    int n = N_ACCURACY;
    std::vector<int32_t> x_in, y_in;
    make_atan2_inputs(n, x_in, y_in);

    std::vector<double> ref(n);
    for (int i = 0; i < n; i++)
        ref[i] = std::atan2(q16_to_dbl(y_in[i]), q16_to_dbl(x_in[i]));

    std::vector<int32_t> fr_out(n), lfm_out(n);
    for (int i = 0; i < n; i++) fr_out[i] = FR_atan2(y_in[i], x_in[i], RADIX);
    for (int i = 0; i < n; i++) lfm_out[i] = fix16_atan2(y_in[i], x_in[i]);

    int ti = 0;
    double fr_ns  = time_fn([&]{ int j = ti++ % n; return FR_atan2(y_in[j], x_in[j], RADIX); });
    ti = 0;
    double lfm_ns = time_fn([&]{ int j = ti++ % n; return fix16_atan2(y_in[j], x_in[j]); });

    return {"atan2", "std::atan2", fr_ns, lfm_ns,
            compute_errors(ref, fr_out), compute_errors(ref, lfm_out),
            "All 4 quadrants", "65536-pt, 5 radii x 360 deg"};
}

static BenchResult bench_sqrt() {
    auto inputs = make_pos_inputs(N_ACCURACY);
    int n = N_ACCURACY;

    std::vector<double> ref(n);
    for (int i = 0; i < n; i++)
        ref[i] = std::sqrt(q16_to_dbl(inputs[i]));

    std::vector<int32_t> fr_out(n), lfm_out(n);
    for (int i = 0; i < n; i++) fr_out[i] = FR_sqrt(inputs[i], RADIX);
    for (int i = 0; i < n; i++) lfm_out[i] = fix16_sqrt(inputs[i]);

    int ti = 0;
    double fr_ns  = time_fn([&]{ return FR_sqrt(inputs[ti++ % n], RADIX); });
    ti = 0;
    double lfm_ns = time_fn([&]{ return fix16_sqrt(inputs[ti++ % n]); });

    return {"sqrt", "std::sqrt", fr_ns, lfm_ns,
            compute_errors(ref, fr_out), compute_errors(ref, lfm_out),
            {}, "65536-pt, [0.01, 100]"};
}

static BenchResult bench_exp() {
    auto inputs = make_exp_inputs(N_ACCURACY);
    int n = N_ACCURACY;

    std::vector<double> ref(n);
    for (int i = 0; i < n; i++)
        ref[i] = std::exp(q16_to_dbl(inputs[i]));

    std::vector<int32_t> fr_out(n), lfm_out(n);
    for (int i = 0; i < n; i++) fr_out[i] = FR_EXP(inputs[i], RADIX);
    for (int i = 0; i < n; i++) lfm_out[i] = fix16_exp(inputs[i]);

    int ti = 0;
    double fr_ns  = time_fn([&]{ return FR_EXP(inputs[ti++ % n], RADIX); });
    ti = 0;
    double lfm_ns = time_fn([&]{ return fix16_exp(inputs[ti++ % n]); });

    return {"exp", "std::exp", fr_ns, lfm_ns,
            compute_errors(ref, fr_out), compute_errors(ref, lfm_out),
            {}, "65536-pt, [-5, 5]"};
}

static BenchResult bench_log() {
    auto inputs = make_pos_inputs(N_ACCURACY);
    int n = N_ACCURACY;

    std::vector<double> ref(n);
    for (int i = 0; i < n; i++)
        ref[i] = std::log(q16_to_dbl(inputs[i]));

    std::vector<int32_t> fr_out(n), lfm_out(n);
    for (int i = 0; i < n; i++) fr_out[i] = FR_ln(inputs[i], RADIX, RADIX);
    for (int i = 0; i < n; i++) lfm_out[i] = fix16_log(inputs[i]);

    int ti = 0;
    double fr_ns  = time_fn([&]{ return FR_ln(inputs[ti++ % n], RADIX, RADIX); });
    ti = 0;
    double lfm_ns = time_fn([&]{ return fix16_log(inputs[ti++ % n]); });

    return {"ln", "std::log", fr_ns, lfm_ns,
            compute_errors(ref, fr_out), compute_errors(ref, lfm_out),
            {}, "65536-pt, [0.01, 100]"};
}

static BenchResult bench_log2() {
    auto inputs = make_pos_inputs(N_ACCURACY);
    int n = N_ACCURACY;

    std::vector<double> ref(n);
    for (int i = 0; i < n; i++)
        ref[i] = std::log2(q16_to_dbl(inputs[i]));

    std::vector<int32_t> fr_out(n), lfm_out(n);
    for (int i = 0; i < n; i++) fr_out[i] = FR_log2(inputs[i], RADIX, RADIX);
    for (int i = 0; i < n; i++) lfm_out[i] = fix16_log2(inputs[i]);

    int ti = 0;
    double fr_ns  = time_fn([&]{ return FR_log2(inputs[ti++ % n], RADIX, RADIX); });
    ti = 0;
    double lfm_ns = time_fn([&]{ return fix16_log2(inputs[ti++ % n]); });

    return {"log2", "std::log2", fr_ns, lfm_ns,
            compute_errors(ref, fr_out), compute_errors(ref, lfm_out),
            {}, "65536-pt, [0.01, 100]"};
}

static BenchResult bench_mul() {
    int n = N_ACCURACY;
    auto a_in = make_arith_inputs(n);
    std::vector<int32_t> b_in(n);
    for (int i = 0; i < n; i++)
        b_in[i] = dbl_to_q16(-2.0 + 4.0 * i / (n - 1));

    std::vector<double> ref(n);
    for (int i = 0; i < n; i++)
        ref[i] = q16_to_dbl(a_in[i]) * q16_to_dbl(b_in[i]);

    std::vector<int32_t> fr_out(n), lfm_out(n);
    for (int i = 0; i < n; i++) fr_out[i] = FR_FixMuls(a_in[i], b_in[i]);
    for (int i = 0; i < n; i++) lfm_out[i] = fix16_mul(a_in[i], b_in[i]);

    int ti = 0;
    double fr_ns  = time_fn([&]{ int j = ti++ % n; return FR_FixMuls(a_in[j], b_in[j]); });
    ti = 0;
    double lfm_ns = time_fn([&]{ int j = ti++ % n; return fix16_mul(a_in[j], b_in[j]); });

    return {"mul", "double a*b", fr_ns, lfm_ns,
            compute_errors(ref, fr_out), compute_errors(ref, lfm_out),
            {}, "65536-pt, a in [-50,50], b in [-2,2]"};
}

static BenchResult bench_div() {
    int n = N_ACCURACY;
    std::vector<int32_t> a_in(n), b_in(n);
    for (int i = 0; i < n; i++) {
        a_in[i] = dbl_to_q16(-50.0 + 100.0 * i / (n - 1));
        b_in[i] = dbl_to_q16(0.5 + 49.5 * i / (n - 1));
    }

    std::vector<double> ref(n);
    for (int i = 0; i < n; i++)
        ref[i] = q16_to_dbl(a_in[i]) / q16_to_dbl(b_in[i]);

    std::vector<int32_t> fr_out(n), lfm_out(n);
    for (int i = 0; i < n; i++) fr_out[i] = FR_DIV(a_in[i], RADIX, b_in[i], RADIX);
    for (int i = 0; i < n; i++) lfm_out[i] = fix16_div(a_in[i], b_in[i]);

    int ti = 0;
    double fr_ns  = time_fn([&]{ int j = ti++ % n; return FR_DIV(a_in[j], RADIX, b_in[j], RADIX); });
    ti = 0;
    double lfm_ns = time_fn([&]{ int j = ti++ % n; return fix16_div(a_in[j], b_in[j]); });

    return {"div", "double a/b", fr_ns, lfm_ns,
            compute_errors(ref, fr_out), compute_errors(ref, lfm_out),
            "Both use 64-bit intermediate", "65536-pt, a/b in [-50,50]/[0.5,50]"};
}

/* --- FR_math-only benchmarks (libfixmath has no equivalent) --- */

static BenchResult bench_hypot() {
    int n = N_ACCURACY;
    std::vector<int32_t> x_in, y_in;
    make_atan2_inputs(n, x_in, y_in);

    std::vector<double> ref(n);
    for (int i = 0; i < n; i++)
        ref[i] = std::hypot(q16_to_dbl(x_in[i]), q16_to_dbl(y_in[i]));

    std::vector<int32_t> fr_out(n);
    for (int i = 0; i < n; i++) fr_out[i] = FR_hypot(x_in[i], y_in[i], RADIX);

    int ti = 0;
    double fr_ns = time_fn([&]{ int j = ti++ % n; return FR_hypot(x_in[j], y_in[j], RADIX); });

    /* dummy lfm_err (all zeros) */
    ErrorStats lfm_err = {};

    return {"hypot", "std::hypot", fr_ns, -1,
            compute_errors(ref, fr_out), lfm_err,
            "FR_math only (libfixmath has no hypot)", "65536-pt, 5 radii x 360 deg"};
}

static BenchResult bench_hypot_fast8() {
    int n = N_ACCURACY;
    std::vector<int32_t> x_in, y_in;
    make_atan2_inputs(n, x_in, y_in);

    std::vector<double> ref(n);
    for (int i = 0; i < n; i++)
        ref[i] = std::hypot(q16_to_dbl(x_in[i]), q16_to_dbl(y_in[i]));

    std::vector<int32_t> fr_out(n);
    for (int i = 0; i < n; i++) fr_out[i] = FR_hypot_fast8(x_in[i], y_in[i]);

    int ti = 0;
    double fr_ns = time_fn([&]{ int j = ti++ % n; return FR_hypot_fast8(x_in[j], y_in[j]); });

    ErrorStats lfm_err = {};

    return {"hypot_fast8", "std::hypot", fr_ns, -1,
            compute_errors(ref, fr_out), lfm_err,
            "FR_math only; shift-only, no multiply", "65536-pt, 5 radii x 360 deg"};
}

/* ================================================================
 * JSON output
 * ================================================================ */

static void emit_json(FILE *f, const std::vector<BenchResult>& results) {
    fprintf(f, "{\n");
    fprintf(f, "  \"description\": \"FR_math vs libfixmath benchmark — both measured against math.h double precision (IEEE 754)\",\n");
    fprintf(f, "  \"gold_standard\": \"<cmath> IEEE 754 double precision (~15 significant digits)\",\n");
    fprintf(f, "  \"fixed_point_format\": \"Q16.16 (s15.16), 1 LSB = %.14e\",\n", Q16_LSB);
    fprintf(f, "  \"accuracy_points\": %d,\n", N_ACCURACY);
    fprintf(f, "  \"timing_iterations\": %d,\n", N_TIMING);
    fprintf(f, "  \"rel_error_threshold\": %.2f,\n", REL_THRESH);
    fprintf(f, "  \"platform\": \"macOS ARM (Apple Silicon)\",\n");
    fprintf(f, "  \"optimization\": \"-O2\",\n");
    fprintf(f, "  \"results\": [\n");

    for (size_t r = 0; r < results.size(); r++) {
        const auto& b = results[r];
        fprintf(f, "    {\n");
        fprintf(f, "      \"function\": \"%s\",\n", b.name.c_str());
        fprintf(f, "      \"double_reference\": \"%s\",\n", b.gold_ref.c_str());
        if (!b.sweep.empty())
            fprintf(f, "      \"sweep\": \"%s\",\n", b.sweep.c_str());

        /* speed */
        fprintf(f, "      \"speed\": {\n");
        fprintf(f, "        \"fr_math_ns_per_call\": %.1f", b.fr_ns_per_call);
        if (b.lfm_ns_per_call >= 0) {
            fprintf(f, ",\n        \"libfixmath_ns_per_call\": %.1f", b.lfm_ns_per_call);
            double speedup = b.lfm_ns_per_call / b.fr_ns_per_call;
            fprintf(f, ",\n        \"fr_math_speedup\": %.2f", speedup);
            const char *faster = (speedup > 1.0) ? "fr_math" : "libfixmath";
            fprintf(f, ",\n        \"faster\": \"%s\"", faster);
        }
        fprintf(f, "\n      },\n");

        /* accuracy — fr_math */
        fprintf(f, "      \"accuracy_vs_double\": {\n");
        fprintf(f, "        \"fr_math\": {\n");
        fprintf(f, "          \"max_abs_error\": %.8e,\n", b.fr_err.max_abs_err);
        fprintf(f, "          \"mean_abs_error\": %.8e,\n", b.fr_err.mean_abs_err);
        fprintf(f, "          \"max_error_lsb\": %.1f,\n", b.fr_err.max_lsb_err);
        fprintf(f, "          \"mean_error_lsb\": %.1f,\n", b.fr_err.mean_lsb_err);
        fprintf(f, "          \"max_rel_error_pct\": %.4f,\n", b.fr_err.max_rel_err);
        fprintf(f, "          \"mean_rel_error_pct\": %.4f\n", b.fr_err.mean_rel_err);
        fprintf(f, "        }");

        if (b.lfm_ns_per_call >= 0) {
            /* accuracy — libfixmath */
            fprintf(f, ",\n        \"libfixmath\": {\n");
            fprintf(f, "          \"max_abs_error\": %.8e,\n", b.lfm_err.max_abs_err);
            fprintf(f, "          \"mean_abs_error\": %.8e,\n", b.lfm_err.mean_abs_err);
            fprintf(f, "          \"max_error_lsb\": %.1f,\n", b.lfm_err.max_lsb_err);
            fprintf(f, "          \"mean_error_lsb\": %.1f,\n", b.lfm_err.mean_lsb_err);
            fprintf(f, "          \"max_rel_error_pct\": %.4f,\n", b.lfm_err.max_rel_err);
            fprintf(f, "          \"mean_rel_error_pct\": %.4f\n", b.lfm_err.mean_rel_err);
            fprintf(f, "        }");

            const char *closer =
                (b.fr_err.max_abs_err < b.lfm_err.max_abs_err) ? "fr_math" :
                (b.fr_err.max_abs_err > b.lfm_err.max_abs_err) ? "libfixmath" : "tie";
            fprintf(f, ",\n        \"closer_to_double\": \"%s\"", closer);
        }
        fprintf(f, "\n      }");

        if (!b.note.empty())
            fprintf(f, ",\n      \"note\": \"%s\"", b.note.c_str());
        fprintf(f, "\n    }%s\n", (r + 1 < results.size()) ? "," : "");
    }

    /* summary (head-to-head only, skip FR_math-only functions) */
    fprintf(f, "  ],\n");
    fprintf(f, "  \"summary\": {\n");

    int fr_speed_wins = 0, lfm_speed_wins = 0;
    int fr_closer = 0, lfm_closer = 0, ties = 0;
    int h2h = 0;
    for (auto& b : results) {
        if (b.lfm_ns_per_call < 0) continue;
        h2h++;
        if (b.fr_ns_per_call < b.lfm_ns_per_call) fr_speed_wins++;
        else lfm_speed_wins++;
        if (b.fr_err.max_abs_err < b.lfm_err.max_abs_err) fr_closer++;
        else if (b.fr_err.max_abs_err > b.lfm_err.max_abs_err) lfm_closer++;
        else ties++;
    }
    fprintf(f, "    \"head_to_head_functions\": %d,\n", h2h);
    fprintf(f, "    \"faster_wins\": { \"fr_math\": %d, \"libfixmath\": %d },\n",
            fr_speed_wins, lfm_speed_wins);
    fprintf(f, "    \"accuracy_wins\": { \"fr_math\": %d, \"libfixmath\": %d, \"tie\": %d },\n",
            fr_closer, lfm_closer, ties);
    fprintf(f, "    \"total_functions_tested\": %d\n", (int)results.size());
    fprintf(f, "  },\n");
    fprintf(f, "  \"notes\": [\n");
    fprintf(f, "    \"All accuracy measured vs IEEE 754 double. Lower = closer to perfect.\",\n");
    fprintf(f, "    \"LSB = Q16.16 least-significant-bit = 1.53e-5. Best possible = 0.5 LSB.\",\n");
    fprintf(f, "    \"Percent errors skip |ref| < %.2f to avoid near-zero division spikes.\",\n", REL_THRESH);
    fprintf(f, "    \"Both libraries use Q16.16 (s15.16): 1.0 = 65536.\",\n");
    fprintf(f, "    \"FR_math trig: BAM + 129-entry LUT + linear interpolation.\",\n");
    fprintf(f, "    \"libfixmath trig: parabolic approximation + 5th-order correction.\",\n");
    fprintf(f, "    \"Timing: min of 3 passes x %d calls; cache-warm.\",\n", N_TIMING);
    fprintf(f, "    \"Speedup > 1.0 means FR_math is faster by that factor.\"\n");
    fprintf(f, "  ],\n");

    /* size comparison — compiled from the Makefile in this directory */
    fprintf(f, "  \"compiled_size_note\": \"Run 'make size' in .compare/ for live numbers. The values below are representative.\",\n");
    fprintf(f, "  \"compiled_size\": {\n");
    fprintf(f, "    \"compiler\": \"clang -O2 (macOS ARM)\",\n");
    fprintf(f, "    \"fr_math\": {\n");
    fprintf(f, "      \"files\": \"FR_math.c (single file)\",\n");
    fprintf(f, "      \"functions\": \"trig(6), inv-trig(4), log/ln/log10, exp/pow2/pow10, exp_fast/pow10_fast, sqrt, hypot(3), waves(6), ADSR(4), print(4), format\",\n");
    fprintf(f, "      \"rom_bytes\": 7722,\n");
    fprintf(f, "      \"ram_bss_bytes\": 0,\n");
    fprintf(f, "      \"note\": \"All tables in const ROM. Zero runtime allocation.\"\n");
    fprintf(f, "    },\n");
    fprintf(f, "    \"libfixmath\": {\n");
    fprintf(f, "      \"files\": \"fix16.c, fix16_sqrt.c, fix16_exp.c, fix16_trig.c, fix16_str.c, uint32.c, fract32.c\",\n");
    fprintf(f, "      \"functions\": \"trig(6), inv-trig(4), log/log2, exp, sqrt, mul/div, str\",\n");
    fprintf(f, "      \"rom_bytes\": 4912,\n");
    fprintf(f, "      \"ram_bss_bytes\": 114688,\n");
    fprintf(f, "      \"rom_bytes_no_cache\": 5476,\n");
    fprintf(f, "      \"ram_bss_bytes_no_cache\": 0,\n");
    fprintf(f, "      \"note\": \"Default mode caches 112 KB of sin/exp LUTs in BSS. FIXMATH_NO_CACHE eliminates RAM but recomputes per call.\"\n");
    fprintf(f, "    }\n");
    fprintf(f, "  }\n");
    fprintf(f, "}\n");
}

/* ================================================================
 * Markdown summary table (to stderr for human reading)
 * ================================================================ */

static void emit_markdown(FILE *f, const std::vector<BenchResult>& results) {
    fprintf(f, "\n## FR_math vs libfixmath — Q16.16 comparison\n\n");
    fprintf(f, "All errors measured vs IEEE 754 double. Pct errors skip |ref| < 0.01.\n\n");

    /* --- Accuracy table --- */
    fprintf(f, "### Accuracy\n\n");
    fprintf(f, "| Function | FR max LSB | FR max %%%% | FR avg %%%% | lfm max LSB | lfm max %%%% | lfm avg %%%% | Winner |\n");
    fprintf(f, "|----------|----------:|---------:|---------:|----------:|---------:|---------:|--------|\n");

    for (auto& b : results) {
        const char *winner;
        if (b.lfm_ns_per_call < 0) {
            winner = "FR only";
            fprintf(f, "| %-15s | %7.1f | %7.4f | %7.4f | %9s | %8s | %8s | %-8s |\n",
                    b.name.c_str(),
                    b.fr_err.max_lsb_err, b.fr_err.max_rel_err, b.fr_err.mean_rel_err,
                    "---", "---", "---", winner);
        } else {
            bool fr_better_abs = b.fr_err.max_abs_err < b.lfm_err.max_abs_err;
            bool lfm_better_abs = b.lfm_err.max_abs_err < b.fr_err.max_abs_err;
            winner = fr_better_abs ? "FR" : lfm_better_abs ? "lfm" : "tie";
            fprintf(f, "| %-15s | %7.1f | %7.4f | %7.4f | %9.1f | %7.4f | %7.4f | %-8s |\n",
                    b.name.c_str(),
                    b.fr_err.max_lsb_err, b.fr_err.max_rel_err, b.fr_err.mean_rel_err,
                    b.lfm_err.max_lsb_err, b.lfm_err.max_rel_err, b.lfm_err.mean_rel_err,
                    winner);
        }
    }

    /* --- Speed table --- */
    fprintf(f, "\n### Speed (ns/call, lower is better)\n\n");
    fprintf(f, "| Function | FR_math | libfixmath | Speedup | Faster |\n");
    fprintf(f, "|----------|--------:|-----------:|--------:|--------|\n");

    for (auto& b : results) {
        if (b.lfm_ns_per_call < 0) {
            fprintf(f, "| %-15s | %6.1f | %10s | %7s | FR only |\n",
                    b.name.c_str(), b.fr_ns_per_call, "---", "---");
        } else {
            double speedup = b.lfm_ns_per_call / b.fr_ns_per_call;
            const char *faster = (speedup > 1.0) ? "FR" : "lfm";
            fprintf(f, "| %-15s | %6.1f | %10.1f | %6.2fx | %-7s |\n",
                    b.name.c_str(), b.fr_ns_per_call, b.lfm_ns_per_call,
                    speedup, faster);
        }
    }

    /* --- Summary --- */
    int fr_speed = 0, lfm_speed = 0, fr_acc = 0, lfm_acc = 0, tie_acc = 0, h2h = 0;
    for (auto& b : results) {
        if (b.lfm_ns_per_call < 0) continue;
        h2h++;
        if (b.fr_ns_per_call < b.lfm_ns_per_call) fr_speed++; else lfm_speed++;
        if (b.fr_err.max_abs_err < b.lfm_err.max_abs_err) fr_acc++;
        else if (b.fr_err.max_abs_err > b.lfm_err.max_abs_err) lfm_acc++;
        else tie_acc++;
    }
    fprintf(f, "\n### Summary (%d head-to-head functions)\n\n", h2h);
    fprintf(f, "- **Speed**: FR_math %d / %d, libfixmath %d / %d\n",
            fr_speed, h2h, lfm_speed, h2h);
    fprintf(f, "- **Accuracy**: FR_math %d / %d, libfixmath %d / %d, tie %d / %d\n",
            fr_acc, h2h, lfm_acc, h2h, tie_acc, h2h);
    fprintf(f, "- Accuracy = %d-pt sweep at Q16.16; timing = min of 3 x %dk calls\n",
            N_ACCURACY, N_TIMING / 1000);

    /* --- Size table --- */
    fprintf(f, "\n### Compiled size (clang -O2, macOS ARM)\n\n");
    fprintf(f, "| | FR_math | libfixmath | lfm (no cache) |\n");
    fprintf(f, "|---|---:|---:|---:|\n");
    fprintf(f, "| Code (text) | 6,888 B | 4,880 B | 5,444 B |\n");
    fprintf(f, "| Tables (ROM) | 834 B | 32 B | 32 B |\n");
    fprintf(f, "| **ROM total** | **7,722 B** | **4,912 B** | **5,476 B** |\n");
    fprintf(f, "| BSS / RAM | **0 B** | **112 KB** | **0 B** |\n");
    fprintf(f, "\n");
    fprintf(f, "FR_math packs trig, inv-trig, log/ln/log10, exp/pow2/pow10, sqrt, hypot(3),\n");
    fprintf(f, "waves(6), ADSR, print into 7.5 KB ROM with zero RAM overhead.\n");
    fprintf(f, "libfixmath (trig, inv-trig, log/log2, exp, sqrt, mul/div, str) is 4.8 KB ROM\n");
    fprintf(f, "but caches 112 KB of sin/exp LUTs in BSS at runtime.\n");
    fprintf(f, "\n");
}

/* ================================================================
 * main
 * ================================================================ */

int main() {
    fprintf(stderr, "Running benchmarks (%d accuracy pts, %d timing iters)...\n",
            N_ACCURACY, N_TIMING);

    std::vector<BenchResult> results;
    results.push_back(bench_sin());   fprintf(stderr, "  sin done\n");
    results.push_back(bench_cos());   fprintf(stderr, "  cos done\n");
    results.push_back(bench_tan());   fprintf(stderr, "  tan done\n");
    results.push_back(bench_asin());  fprintf(stderr, "  asin done\n");
    results.push_back(bench_acos());  fprintf(stderr, "  acos done\n");
    results.push_back(bench_atan());  fprintf(stderr, "  atan done\n");
    results.push_back(bench_atan2()); fprintf(stderr, "  atan2 done\n");
    results.push_back(bench_sqrt());  fprintf(stderr, "  sqrt done\n");
    results.push_back(bench_exp());   fprintf(stderr, "  exp done\n");
    results.push_back(bench_log());   fprintf(stderr, "  ln done\n");
    results.push_back(bench_log2());  fprintf(stderr, "  log2 done\n");
    results.push_back(bench_mul());   fprintf(stderr, "  mul done\n");
    results.push_back(bench_div());   fprintf(stderr, "  div done\n");
    results.push_back(bench_hypot()); fprintf(stderr, "  hypot done\n");
    results.push_back(bench_hypot_fast8()); fprintf(stderr, "  hypot_fast8 done\n");

    emit_json(stdout, results);
    emit_markdown(stderr, results);
    return 0;
}
