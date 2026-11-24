/*
 * benchmark.cpp — FR_math vs libfixmath comparison
 *
 * WARNING: This file lives in compare_lfm/ and must NOT touch any
 *          files outside this directory.
 *
 * Both libraries use Q16.16 (s15.16) fixed-point: 1.0 = 65536.
 * We compare: speed (ns per call) and accuracy (vs double).
 *
 * Accuracy: every Q16.16 input in each function's domain (see the
 * "sweep" column); 2-input functions use every pair on a stated grid.
 * Nothing in a domain is skipped: zero crossings, tan poles and domain
 * edges are all included.
 *
 * Error measure: |got - exact| / max(|exact|, 1.0), in %. For outputs
 * with |exact| <= 1 that is the absolute error (1 LSB = 0.0015 %); above
 * 1 it is the relative error. Where the exact value is outside the
 * Q16.16 range (tan near a pole), a saturated result of the right sign
 * (|got| >= 0x7FFF0000, or libfixmath's fix16_overflow sentinel for a
 * negative value) counts as exact; those points are counted in "sat".
 * Max LSB = max |got - exact| in Q16.16 LSBs (huge for tan near poles in
 * both libraries, since the exact value there is huge).
 *
 * Timing: mean ns/call over the whole input set, min of 3 passes.
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
#include <functional>

/* ---- FR_math ---- */
extern "C" {
#include "FR_defs.h"
#include "FR_math.h"
}

/* ---- libfixmath ---- */
#include "fixmath.h"

static const int RADIX = 16;                    /* s15.16 */
static const double ONE = 65536.0;
static const double Q16_LSB = 1.0 / ONE;

static inline double q16_to_dbl(int32_t v) { return (double)v / ONE; }
static inline int32_t dbl_to_q16(double d) { return (int32_t)std::lround(d * ONE); }

static inline int64_t now_ns() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

/* ================================================================
 * Inputs
 * ================================================================ */

/* every raw Q16.16 value in [lo, hi] */
static std::vector<int32_t> every(double lo, double hi) {
    int64_t a = (int64_t)std::ceil(lo * ONE), b = (int64_t)std::floor(hi * ONE);
    std::vector<int32_t> v;
    v.reserve((size_t)(b - a + 1));
    for (int64_t x = a; x <= b; x++) v.push_back((int32_t)x);
    return v;
}

/* every (a, b) on an n x n grid over [alo, ahi] x [blo, bhi] */
static void grid(int n, double alo, double ahi, double blo, double bhi,
                 std::vector<int32_t>& a, std::vector<int32_t>& b) {
    a.clear(); b.clear();
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++) {
            a.push_back(dbl_to_q16(alo + (ahi - alo) * i / (n - 1)));
            b.push_back(dbl_to_q16(blo + (bhi - blo) * j / (n - 1)));
        }
}

/* every raw (y, x) in [-512, 511]^2, times 2^0, 2^8 and 2^14 (magnitudes
 * from 1 LSB to 512.0), for atan2 and hypot */
static void pairs(std::vector<int32_t>& y, std::vector<int32_t>& x) {
    y.clear(); x.clear();
    static const int scale[] = {0, 8, 14};
    for (int s : scale)
        for (int32_t i = -512; i < 512; i++)
            for (int32_t j = -512; j < 512; j++) {
                y.push_back(i * (1 << s));
                x.push_back(j * (1 << s));
            }
}

/* ================================================================
 * Error measurement
 * ================================================================ */

struct ErrorStats {
    double max_pct = 0, mean_pct = 0;   /* |err| / max(|exact|, 1), % */
    double max_lsb = 0;                 /* |err| in Q16.16 LSBs */
    double worst_in = 0;                /* input at max_pct */
    long   count = 0, sat = 0;
};

static ErrorStats compute_errors(const std::vector<double>& ref,
                                 const std::vector<int32_t>& got,
                                 const std::vector<double>& in) {
    ErrorStats s;
    double sum = 0;
    for (size_t i = 0; i < ref.size(); i++) {
        double e;
        if (std::fabs(ref[i]) * ONE >= 2147483647.0) {
            int32_t g = got[i];
            bool ok = (ref[i] > 0) ? (g >= 0x7FFF0000)
                                   : (g <= -0x7FFF0000 || g == (int32_t)0x80000000);
            s.sat++;
            e = ok ? 0.0 : 100.0;
        } else {
            double d = std::fabs(q16_to_dbl(got[i]) - ref[i]);
            double lsb = d / Q16_LSB;
            if (lsb > s.max_lsb) s.max_lsb = lsb;
            e = 100.0 * d / std::max(std::fabs(ref[i]), 1.0);
        }
        sum += e;
        if (e > s.max_pct) { s.max_pct = e; s.worst_in = in[i]; }
    }
    s.count = (long)ref.size();
    s.mean_pct = s.count ? sum / s.count : 0;
    return s;
}

/* ================================================================
 * Runner
 * ================================================================ */

struct BenchResult {
    std::string name, gold_ref;
    double fr_ns = 0, lfm_ns = -1;      /* lfm_ns < 0: no libfixmath equivalent */
    ErrorStats fr_err, lfm_err;
    std::string note, sweep;
};

static volatile int32_t sink;

/* mean ns/call over all n inputs; min of 3 passes */
static double time_all(size_t n, const std::function<int32_t(size_t)>& fn) {
    for (size_t i = 0; i < std::min<size_t>(n, 4096); i++) sink = fn(i);   /* warm-up */
    double best = 1e18;
    for (int pass = 0; pass < 3; pass++) {
        int64_t t0 = now_ns();
        for (size_t i = 0; i < n; i++) sink = fn(i);
        double ns = (double)(now_ns() - t0) / (double)n;
        best = std::min(best, ns);
    }
    return best;
}


/* one-input function */
static BenchResult run1(const char *name, const char *gold, const char *sweep,
                        const std::vector<int32_t>& in, double (*ref_fn)(double),
                        const std::function<int32_t(int32_t)>& fr,
                        const std::function<int32_t(int32_t)>& lfm,
                        const char *note = "") {
    size_t n = in.size();
    std::vector<double> ref(n), x(n);
    std::vector<int32_t> fo(n), lo(n);
    for (size_t i = 0; i < n; i++) { x[i] = q16_to_dbl(in[i]); ref[i] = ref_fn(x[i]); }
    for (size_t i = 0; i < n; i++) fo[i] = fr(in[i]);
    BenchResult b;
    b.name = name; b.gold_ref = gold; b.sweep = sweep; b.note = note;
    b.fr_err = compute_errors(ref, fo, x);
    b.fr_ns = time_all(n, [&](size_t i) { return fr(in[i]); });
    if (lfm) {
        for (size_t i = 0; i < n; i++) lo[i] = lfm(in[i]);
        b.lfm_err = compute_errors(ref, lo, x);
        b.lfm_ns = time_all(n, [&](size_t i) { return lfm(in[i]); });
    }
    return b;
}

/* two-input function */
static BenchResult run2(const char *name, const char *gold, const char *sweep,
                        const std::vector<int32_t>& a, const std::vector<int32_t>& c,
                        double (*ref_fn)(double, double),
                        const std::function<int32_t(int32_t, int32_t)>& fr,
                        const std::function<int32_t(int32_t, int32_t)>& lfm,
                        const char *note = "") {
    size_t n = a.size();
    std::vector<double> ref(n), x(n);
    std::vector<int32_t> fo(n), lo(n);
    for (size_t i = 0; i < n; i++) { x[i] = q16_to_dbl(a[i]); ref[i] = ref_fn(q16_to_dbl(a[i]), q16_to_dbl(c[i])); }
    for (size_t i = 0; i < n; i++) fo[i] = fr(a[i], c[i]);
    BenchResult b;
    b.name = name; b.gold_ref = gold; b.sweep = sweep; b.note = note;
    b.fr_err = compute_errors(ref, fo, x);
    b.fr_ns = time_all(n, [&](size_t i) { return fr(a[i], c[i]); });
    if (lfm) {
        for (size_t i = 0; i < n; i++) lo[i] = lfm(a[i], c[i]);
        b.lfm_err = compute_errors(ref, lo, x);
        b.lfm_ns = time_all(n, [&](size_t i) { return lfm(a[i], c[i]); });
    }
    return b;
}

static double d_mul(double a, double b) { return a * b; }
static double d_div(double a, double b) { return a / b; }
static double d_atan2(double y, double x) { return std::atan2(y, x); }
static double d_hypot(double x, double y) { return std::hypot(x, y); }
static double d_tan(double x) { return std::tan(x); }

/* ================================================================
 * Output
 * ================================================================ */

static void emit_json(FILE *f, const std::vector<BenchResult>& results) {
    fprintf(f, "{\n");
    fprintf(f, "  \"description\": \"FR_math vs libfixmath, every Q16.16 input in each domain, vs IEEE 754 double\",\n");
    fprintf(f, "  \"fixed_point_format\": \"Q16.16 (s15.16), 1 LSB = %.14e\",\n", Q16_LSB);
    fprintf(f, "  \"error_measure\": \"|got - exact| / max(|exact|, 1.0) in %%; saturated results beyond the Q16.16 range count as exact\",\n");
    fprintf(f, "  \"timing\": \"mean ns/call over the whole input set, min of 3 passes\",\n");
    fprintf(f, "  \"results\": [\n");
    for (size_t r = 0; r < results.size(); r++) {
        const BenchResult& b = results[r];
        fprintf(f, "    {\n      \"function\": \"%s\",\n      \"double_reference\": \"%s\",\n", b.name.c_str(), b.gold_ref.c_str());
        fprintf(f, "      \"sweep\": \"%s\",\n      \"inputs\": %ld,\n", b.sweep.c_str(), b.fr_err.count);
        fprintf(f, "      \"fr_math\": { \"ns_per_call\": %.2f, \"max_err_pct\": %.6f, \"mean_err_pct\": %.6f, \"max_err_lsb\": %.1f, \"worst_input\": %.8f, \"saturated_points\": %ld }",
                b.fr_ns, b.fr_err.max_pct, b.fr_err.mean_pct, b.fr_err.max_lsb, b.fr_err.worst_in, b.fr_err.sat);
        if (b.lfm_ns >= 0)
            fprintf(f, ",\n      \"libfixmath\": { \"ns_per_call\": %.2f, \"max_err_pct\": %.6f, \"mean_err_pct\": %.6f, \"max_err_lsb\": %.1f, \"worst_input\": %.8f, \"saturated_points\": %ld }",
                    b.lfm_ns, b.lfm_err.max_pct, b.lfm_err.mean_pct, b.lfm_err.max_lsb, b.lfm_err.worst_in, b.lfm_err.sat);
        if (!b.note.empty()) fprintf(f, ",\n      \"note\": \"%s\"", b.note.c_str());
        fprintf(f, "\n    }%s\n", (r + 1 < results.size()) ? "," : "");
    }
    fprintf(f, "  ]\n}\n");
}

static void emit_markdown(FILE *f, const std::vector<BenchResult>& results) {
    fprintf(f, "\n## FR_math vs libfixmath — Q16.16 comparison\n\n");
    fprintf(f, "Every Q16.16 input in each domain (2-input functions: every pair on the stated grid),\n");
    fprintf(f, "vs IEEE 754 double. Nothing is skipped. Error = |got - exact| / max(|exact|, 1.0);\n");
    fprintf(f, "beyond the Q16.16 range (tan at its poles) a saturated result of the right sign counts as exact.\n\n");

    fprintf(f, "### Accuracy\n\n");
    fprintf(f, "| Function | Inputs | Sweep | FR max %% | FR avg %% | lfm max %% | lfm avg %% | More accurate |\n");
    fprintf(f, "|----------|-------:|-------|---------:|---------:|----------:|----------:|---------------|\n");
    for (const BenchResult& b : results) {
        if (b.lfm_ns < 0) {
            fprintf(f, "| %s | %ld | %s | %.4f | %.4f | --- | --- | FR only |\n",
                    b.name.c_str(), b.fr_err.count, b.sweep.c_str(), b.fr_err.max_pct, b.fr_err.mean_pct);
            continue;
        }
        const char *w = (b.fr_err.max_pct < b.lfm_err.max_pct) ? "FR"
                      : (b.fr_err.max_pct > b.lfm_err.max_pct) ? "lfm" : "tie";
        fprintf(f, "| %s | %ld | %s | %.4f | %.4f | %.4f | %.4f | %s |\n",
                b.name.c_str(), b.fr_err.count, b.sweep.c_str(),
                b.fr_err.max_pct, b.fr_err.mean_pct, b.lfm_err.max_pct, b.lfm_err.mean_pct, w);
    }

    fprintf(f, "\n### Speed (mean ns/call over the input set, lower is better)\n\n");
    fprintf(f, "| Function | FR_math | libfixmath | Speedup | Faster |\n");
    fprintf(f, "|----------|--------:|-----------:|--------:|--------|\n");
    for (const BenchResult& b : results) {
        if (b.lfm_ns < 0) {
            fprintf(f, "| %s | %.1f | --- | --- | FR only |\n", b.name.c_str(), b.fr_ns);
            continue;
        }
        double sp = b.lfm_ns / b.fr_ns;
        fprintf(f, "| %s | %.1f | %.1f | %.2fx | %s |\n", b.name.c_str(), b.fr_ns, b.lfm_ns, sp, sp > 1.0 ? "FR" : "lfm");
    }

    int h2h = 0, frs = 0, lfs = 0, fra = 0, lfa = 0, tie = 0;
    for (const BenchResult& b : results) {
        if (b.lfm_ns < 0) continue;
        h2h++;
        if (b.fr_ns < b.lfm_ns) frs++; else lfs++;
        if (b.fr_err.max_pct < b.lfm_err.max_pct) fra++;
        else if (b.fr_err.max_pct > b.lfm_err.max_pct) lfa++;
        else tie++;
    }
    fprintf(f, "\n### Summary (%d head-to-head functions)\n\n", h2h);
    fprintf(f, "- **Speed**: FR_math %d / %d, libfixmath %d / %d\n", frs, h2h, lfs, h2h);
    fprintf(f, "- **Accuracy (max error)**: FR_math %d / %d, libfixmath %d / %d, tie %d / %d\n", fra, h2h, lfa, h2h, tie, h2h);
    fprintf(f, "\nWorst-case inputs (FR / lfm):\n\n");
    for (const BenchResult& b : results) {
        if (b.lfm_ns < 0) fprintf(f, "- %s: %.8g\n", b.name.c_str(), b.fr_err.worst_in);
        else fprintf(f, "- %s: %.8g / %.8g\n", b.name.c_str(), b.fr_err.worst_in, b.lfm_err.worst_in);
    }
    fprintf(f, "\nCompiled size: run `make size` in this directory.\n");
}

/* ================================================================
 * main
 * ================================================================ */

int main() {
    std::vector<BenchResult> R;
    std::vector<int32_t> a, b;
    const double PI2 = 2.0 * M_PI;

    fprintf(stderr, "Running benchmarks (every input per domain)...\n");
    {
        auto in = every(-PI2, PI2);
        R.push_back(run1("sin", "std::sin", "every input in [-2pi, 2pi]", in, std::sin,
                         [](int32_t x) { return fr_sin(x, RADIX); }, [](int32_t x) { return fix16_sin(x); }));
        fprintf(stderr, "  sin done\n");
        R.push_back(run1("cos", "std::cos", "every input in [-2pi, 2pi]", in, std::cos,
                         [](int32_t x) { return fr_cos(x, RADIX); }, [](int32_t x) { return fix16_cos(x); }));
        fprintf(stderr, "  cos done\n");
        R.push_back(run1("tan", "std::tan", "every input in [-2pi, 2pi], poles included", in, d_tan,
                         [](int32_t x) { return fr_tan(x, RADIX); }, [](int32_t x) { return fix16_tan(x); }));
        fprintf(stderr, "  tan done\n");
    }
    {
        auto in = every(-1.0, 1.0);
        R.push_back(run1("asin", "std::asin", "every input in [-1, 1]", in, std::asin,
                         [](int32_t x) { return FR_asin(x, RADIX, RADIX); }, [](int32_t x) { return fix16_asin(x); }));
        fprintf(stderr, "  asin done\n");
        R.push_back(run1("acos", "std::acos", "every input in [-1, 1]", in, std::acos,
                         [](int32_t x) { return FR_acos(x, RADIX, RADIX); }, [](int32_t x) { return fix16_acos(x); }));
        fprintf(stderr, "  acos done\n");
    }
    {
        auto in = every(-64.0, 64.0);
        R.push_back(run1("atan", "std::atan", "every input in [-64, 64]", in, std::atan,
                         [](int32_t x) { return FR_atan(x, RADIX, RADIX); }, [](int32_t x) { return fix16_atan(x); }));
        fprintf(stderr, "  atan done\n");
    }
    {
        pairs(a, b);
        R.push_back(run2("atan2", "std::atan2", "every (y, x) in [-512, 511]^2 LSB, x 2^0, 2^8, 2^14", a, b, d_atan2,
                         [](int32_t y, int32_t x) { return FR_atan2(y, x, RADIX); },
                         [](int32_t y, int32_t x) { return fix16_atan2(y, x); }));
        fprintf(stderr, "  atan2 done\n");
    }
    {
        auto in = every(0.0, 1024.0);
        R.push_back(run1("sqrt", "std::sqrt", "every input in [0, 1024]", in, std::sqrt,
                         [](int32_t x) { return FR_sqrt(x, RADIX); }, [](int32_t x) { return fix16_sqrt(x); }));
        fprintf(stderr, "  sqrt done\n");
    }
    {
        /* exp(x) fits in Q16.16 for x < ln(32768) = 10.397 */
        auto in = every(-16.0, 10.397);
        R.push_back(run1("exp", "std::exp", "every input in [-16, 10.397]", in, std::exp,
                         [](int32_t x) { return FR_EXP(x, RADIX); }, [](int32_t x) { return fix16_exp(x); }));
        fprintf(stderr, "  exp done\n");
    }
    {
        auto in = every(Q16_LSB, 1024.0);
        R.push_back(run1("ln", "std::log", "every input in (0, 1024]", in, std::log,
                         [](int32_t x) { return FR_ln(x, RADIX, RADIX); }, [](int32_t x) { return fix16_log(x); }));
        fprintf(stderr, "  ln done\n");
        R.push_back(run1("log2", "std::log2", "every input in (0, 1024]", in, std::log2,
                         [](int32_t x) { return FR_log2(x, RADIX, RADIX); }, [](int32_t x) { return fix16_log2(x); }));
        fprintf(stderr, "  log2 done\n");
    }
    {
        grid(2048, -50.0, 50.0, -2.0, 2.0, a, b);
        R.push_back(run2("mul", "double a*b", "2048x2048 grid, a in [-50,50], b in [-2,2]", a, b, d_mul,
                         [](int32_t x, int32_t y) { return FR_FixMuls(x, y); },
                         [](int32_t x, int32_t y) { return fix16_mul(x, y); }));
        fprintf(stderr, "  mul done\n");
        grid(2048, -50.0, 50.0, 0.5, 50.0, a, b);
        R.push_back(run2("div", "double a/b", "2048x2048 grid, a in [-50,50], b in [0.5,50]", a, b, d_div,
                         [](int32_t x, int32_t y) { return FR_DIV(x, RADIX, y, RADIX); },
                         [](int32_t x, int32_t y) { return fix16_div(x, y); }));
        fprintf(stderr, "  div done\n");
    }
    {
        pairs(a, b);
        R.push_back(run2("hypot", "std::hypot", "every (x, y) in [-512, 511]^2 LSB, x 2^0, 2^8, 2^14", a, b, d_hypot,
                         [](int32_t x, int32_t y) { return FR_hypot(x, y, RADIX); }, nullptr,
                         "FR_math only (libfixmath has no hypot)"));
        R.push_back(run2("hypot_fast8", "std::hypot", "every (x, y) in [-512, 511]^2 LSB, x 2^0, 2^8, 2^14", a, b, d_hypot,
                         [](int32_t x, int32_t y) { return FR_hypot_fast8(x, y); }, nullptr,
                         "FR_math only; shift-only, no multiply"));
        fprintf(stderr, "  hypot done\n");
    }
    emit_json(stdout, R);
    emit_markdown(stderr, R);
    return 0;
}
