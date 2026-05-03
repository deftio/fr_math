/*
 * trig_neighborhood.cpp — sweep any math function over a range, print neighborhood table
 *
 * Usage:
 *   trig_neighborhood <func> <center> <half> [--inc <step>] [--fmt md|csv|ascii]
 *                     [--radix <r>] [--out_radix <r>] [--y <val>]
 *
 * Trig functions:
 *   fr_sin_bam, fr_cos_bam, fr_tan_bam,
 *   fr_sin, fr_cos, fr_tan,
 *   FR_SinI, FR_CosI, FR_TanI,
 *   fr_sin_deg, fr_cos_deg, fr_tan_deg
 *
 * Inverse trig:
 *   FR_acos, FR_asin, FR_atan, FR_atan2
 *
 * Logarithmic:
 *   FR_log2, FR_ln, FR_log10
 *
 * Exponential:
 *   FR_pow2, FR_EXP, FR_POW10
 *
 * Other:
 *   FR_sqrt, FR_hypot, FR_hypot_fast8
 *
 * center:       center value (degrees for trig/atan2, input value for others)
 * half:         number of samples on each side of center
 * --inc:        increment (default depends on function type)
 * --fmt:        output format: md (default), csv, ascii
 * --radix:      input radix for fixed-point functions (default: 16)
 * --out_radix:  output radix for inverse trig and log (default: 16)
 * --y:          fixed y value for FR_hypot / FR_hypot_fast8 (default: 0.0)
 *
 * Examples:
 *   trig_neighborhood fr_cos -90 15
 *   trig_neighborhood fr_sin -360 10 --fmt csv
 *   trig_neighborhood fr_tan 89.5 20 --inc 0.01
 *   trig_neighborhood fr_sin_deg 45 10 --radix 8
 *   trig_neighborhood FR_asin 0.5 15 --radix 15 --out_radix 16
 *   trig_neighborhood FR_log2 1.0 15 --inc 0.01
 *   trig_neighborhood FR_atan2 90 15
 *   trig_neighborhood FR_hypot_fast8 100 15 --y 50 --radix 8
 *
 * Build:
 *   make tools
 */
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include "FR_math.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static double frd(s32 v, int p) { return (double)v / ldexp(1.0, p); }
static double qN(double v, int p) { double s = ldexp(1.0, p); return floor(v * s + 0.5) / s; }
/* Round-to-nearest float→fixed conversion (not truncation) */
static s32 tofix(double v, int p) { return (s32)floor(ldexp(v, p) + 0.5); }
static const double TAN_CLAMP = (double)0x7fffffff / 65536.0;
static double tan_ref(double rad) {
    double t = tan(rad);
    if (t >  TAN_CLAMP) return  TAN_CLAMP;
    if (t < -TAN_CLAMP) return -TAN_CLAMP;
    return t;
}

enum Func {
    F_SIN_BAM, F_COS_BAM, F_TAN_BAM,
    F_SIN,     F_COS,     F_TAN,
    F_SINI,    F_COSI,    F_TANI,
    F_SIN_DEG, F_COS_DEG, F_TAN_DEG,
    F_ACOS, F_ASIN, F_ATAN, F_ATAN2,
    F_LOG2, F_LN, F_LOG10,
    F_POW2, F_EXP, F_POW10,
    F_SQRT, F_HYPOT, F_HYPOT_FAST8,
    F_UNKNOWN
};

enum Fmt { FMT_MD, FMT_CSV, FMT_ASCII };

static Func parse_func(const char *s) {
    if (!strcmp(s, "fr_sin_bam"))      return F_SIN_BAM;
    if (!strcmp(s, "fr_cos_bam"))      return F_COS_BAM;
    if (!strcmp(s, "fr_tan_bam"))      return F_TAN_BAM;
    if (!strcmp(s, "fr_sin"))          return F_SIN;
    if (!strcmp(s, "fr_cos"))          return F_COS;
    if (!strcmp(s, "fr_tan"))          return F_TAN;
    if (!strcmp(s, "FR_SinI"))         return F_SINI;
    if (!strcmp(s, "FR_CosI"))         return F_COSI;
    if (!strcmp(s, "FR_TanI"))         return F_TANI;
    if (!strcmp(s, "fr_sin_deg"))      return F_SIN_DEG;
    if (!strcmp(s, "fr_cos_deg"))      return F_COS_DEG;
    if (!strcmp(s, "fr_tan_deg"))      return F_TAN_DEG;
    if (!strcmp(s, "FR_acos"))         return F_ACOS;
    if (!strcmp(s, "FR_asin"))         return F_ASIN;
    if (!strcmp(s, "FR_atan"))         return F_ATAN;
    if (!strcmp(s, "FR_atan2"))        return F_ATAN2;
    if (!strcmp(s, "FR_log2"))         return F_LOG2;
    if (!strcmp(s, "FR_ln"))           return F_LN;
    if (!strcmp(s, "FR_log10"))        return F_LOG10;
    if (!strcmp(s, "FR_pow2"))         return F_POW2;
    if (!strcmp(s, "FR_EXP"))          return F_EXP;
    if (!strcmp(s, "FR_POW10"))        return F_POW10;
    if (!strcmp(s, "FR_sqrt"))         return F_SQRT;
    if (!strcmp(s, "FR_hypot"))        return F_HYPOT;
    if (!strcmp(s, "FR_hypot_fast8"))  return F_HYPOT_FAST8;
    return F_UNKNOWN;
}

static const char *func_name(Func f) {
    switch (f) {
    case F_SIN_BAM:     return "fr_sin_bam";
    case F_COS_BAM:     return "fr_cos_bam";
    case F_TAN_BAM:     return "fr_tan_bam";
    case F_SIN:         return "fr_sin";
    case F_COS:         return "fr_cos";
    case F_TAN:         return "fr_tan";
    case F_SINI:        return "FR_SinI";
    case F_COSI:        return "FR_CosI";
    case F_TANI:        return "FR_TanI";
    case F_SIN_DEG:     return "fr_sin_deg";
    case F_COS_DEG:     return "fr_cos_deg";
    case F_TAN_DEG:     return "fr_tan_deg";
    case F_ACOS:        return "FR_acos";
    case F_ASIN:        return "FR_asin";
    case F_ATAN:        return "FR_atan";
    case F_ATAN2:       return "FR_atan2";
    case F_LOG2:        return "FR_log2";
    case F_LN:          return "FR_ln";
    case F_LOG10:       return "FR_log10";
    case F_POW2:        return "FR_pow2";
    case F_EXP:         return "FR_EXP";
    case F_POW10:       return "FR_POW10";
    case F_SQRT:        return "FR_sqrt";
    case F_HYPOT:       return "FR_hypot";
    case F_HYPOT_FAST8: return "FR_hypot_fast8";
    default:            return "?";
    }
}

static int is_sin(Func f) { return f == F_SIN_BAM || f == F_SIN || f == F_SINI || f == F_SIN_DEG; }
static int is_cos(Func f) { return f == F_COS_BAM || f == F_COS || f == F_COSI || f == F_COS_DEG; }
static int is_trig(Func f) { return f <= F_TAN_DEG; }

/* Evaluate function. Returns raw s32 result and sets input_fp, expected, out_prec. */
static s32 eval(Func f, double val, int radix, int out_radix,
                double y_val, s32 *input_fp, double *expected, int *out_prec)
{
    s32 raw = 0;

    /* --- Trig functions (val = degrees) --- */
    if (is_trig(f)) {
        double rad = val * M_PI / 180.0;
        *out_prec = 16;

        if (is_sin(f)) *expected = qN(sin(rad), 16);
        else if (is_cos(f)) *expected = qN(cos(rad), 16);
        else *expected = qN(tan_ref(rad), 16);

        switch (f) {
        case F_SIN_BAM: {
            u16 bam = (u16)((int)(val * 65536.0 / 360.0 + 0.5) & 0xFFFF);
            *input_fp = (s32)bam;
            raw = fr_sin_bam(bam);
            break;
        }
        case F_COS_BAM: {
            u16 bam = (u16)((int)(val * 65536.0 / 360.0 + 0.5) & 0xFFFF);
            *input_fp = (s32)bam;
            raw = fr_cos_bam(bam);
            break;
        }
        case F_TAN_BAM: {
            u16 bam = (u16)((int)(val * 65536.0 / 360.0 + 0.5) & 0xFFFF);
            *input_fp = (s32)bam;
            raw = fr_tan_bam(bam);
            break;
        }
        case F_SIN: {
            s32 rad_fp = tofix(rad, radix);
            *input_fp = rad_fp;
            raw = fr_sin(rad_fp, (u16)radix);
            break;
        }
        case F_COS: {
            s32 rad_fp = tofix(rad, radix);
            *input_fp = rad_fp;
            raw = fr_cos(rad_fp, (u16)radix);
            break;
        }
        case F_TAN: {
            s32 rad_fp = tofix(rad, radix);
            *input_fp = rad_fp;
            raw = fr_tan(rad_fp, (u16)radix);
            break;
        }
        case F_SINI:
            *input_fp = (s32)(int)val;
            raw = FR_SinI((int)val);
            break;
        case F_COSI:
            *input_fp = (s32)(int)val;
            raw = FR_CosI((int)val);
            break;
        case F_TANI:
            *input_fp = (s32)(int)val;
            raw = FR_TanI((s16)(int)val);
            break;
        case F_SIN_DEG: {
            s32 deg_fp = tofix(val, radix);
            *input_fp = deg_fp;
            raw = fr_sin_deg(deg_fp, (u16)radix);
            break;
        }
        case F_COS_DEG: {
            s32 deg_fp = tofix(val, radix);
            *input_fp = deg_fp;
            raw = fr_cos_deg(deg_fp, (u16)radix);
            break;
        }
        case F_TAN_DEG: {
            s32 deg_fp = tofix(val, radix);
            *input_fp = deg_fp;
            raw = fr_tan_deg(deg_fp, (u16)radix);
            break;
        }
        default:
            break;
        }
        return raw;
    }

    /* --- Inverse trig (val = input value, not degrees) --- */
    if (f == F_ACOS || f == F_ASIN || f == F_ATAN) {
        *out_prec = out_radix;
        s32 inp = tofix(val, radix);
        *input_fp = inp;

        switch (f) {
        case F_ACOS:
            raw = FR_acos(inp, (u16)radix, (u16)out_radix);
            *expected = qN(acos(val), out_radix);
            break;
        case F_ASIN:
            raw = FR_asin(inp, (u16)radix, (u16)out_radix);
            *expected = qN(asin(val), out_radix);
            break;
        case F_ATAN:
            raw = FR_atan(inp, (u16)radix, (u16)out_radix);
            *expected = qN(atan(val), out_radix);
            break;
        default:
            break;
        }
        return raw;
    }

    /* --- FR_atan2 (val = degrees on unit circle) --- */
    if (f == F_ATAN2) {
        *out_prec = out_radix;
        double rad = val * M_PI / 180.0;
        s32 x = tofix(cos(rad), 15);
        s32 y = tofix(sin(rad), 15);
        *input_fp = tofix(val, radix);
        raw = FR_atan2(y, x, (u16)out_radix);
        double ref = atan2((double)y, (double)x);
        *expected = qN(ref, out_radix);
        return raw;
    }

    /* --- Log functions (val = input value) --- */
    if (f == F_LOG2 || f == F_LN || f == F_LOG10) {
        *out_prec = out_radix;
        s32 inp = tofix(val, radix);
        *input_fp = inp;

        switch (f) {
        case F_LOG2:
            raw = FR_log2(inp, (u16)radix, (u16)out_radix);
            *expected = (val > 0.0) ? qN(log2(val), out_radix) : 0.0;
            break;
        case F_LN:
            raw = FR_ln(inp, (u16)radix, (u16)out_radix);
            *expected = (val > 0.0) ? qN(log(val), out_radix) : 0.0;
            break;
        case F_LOG10:
            raw = FR_log10(inp, (u16)radix, (u16)out_radix);
            *expected = (val > 0.0) ? qN(log10(val), out_radix) : 0.0;
            break;
        default:
            break;
        }
        return raw;
    }

    /* --- Power/exp functions (val = exponent) --- */
    if (f == F_POW2 || f == F_EXP || f == F_POW10) {
        *out_prec = radix;
        s32 inp = tofix(val, radix);
        *input_fp = inp;

        switch (f) {
        case F_POW2:
            raw = FR_pow2(inp, (u16)radix);
            *expected = qN(pow(2.0, val), radix);
            break;
        case F_EXP:
            raw = FR_EXP(inp, (u16)radix);
            *expected = qN(exp(val), radix);
            break;
        case F_POW10:
            raw = FR_POW10(inp, (u16)radix);
            *expected = qN(pow(10.0, val), radix);
            break;
        default:
            break;
        }
        return raw;
    }

    /* --- FR_sqrt (val = input value) --- */
    if (f == F_SQRT) {
        *out_prec = radix;
        s32 inp = tofix(val, radix);
        *input_fp = inp;
        raw = FR_sqrt(inp, (u16)radix);
        *expected = (val >= 0.0) ? qN(sqrt(val), radix) : 0.0;
        return raw;
    }

    /* --- FR_hypot / FR_hypot_fast8 (val = x, y_val = y) --- */
    if (f == F_HYPOT || f == F_HYPOT_FAST8) {
        *out_prec = radix;
        s32 x_fp = tofix(val, radix);
        s32 y_fp = tofix(y_val, radix);
        *input_fp = x_fp;

        if (f == F_HYPOT)
            raw = FR_hypot(x_fp, y_fp, (u16)radix);
        else
            raw = FR_hypot_fast8(x_fp, y_fp);

        *expected = qN(hypot(val, y_val), radix);
        return raw;
    }

    /* fallback */
    *input_fp = 0;
    *expected = 0.0;
    *out_prec = 16;
    return 0;
}

/* Smart default increment based on function type */
static double default_inc(Func f) {
    if (is_trig(f) || f == F_ATAN2)
        return 360.0 / 65536.0;  /* ~0.0055 degrees */
    if (f == F_ACOS || f == F_ASIN)
        return 1.0 / 32768.0;    /* ~3.05e-5, matches r15 LSB */
    return 1.0 / 65536.0;        /* ~1.53e-5, matches r16 LSB */
}

static void usage(void) {
    fprintf(stderr,
        "Usage: trig_neighborhood <func> <center> <half> [options]\n"
        "\n"
        "Supported functions:\n"
        "\n"
        "  Trig (input: degrees):\n"
        "    fr_sin_bam, fr_cos_bam, fr_tan_bam\n"
        "    fr_sin, fr_cos, fr_tan\n"
        "    FR_SinI, FR_CosI, FR_TanI\n"
        "    fr_sin_deg, fr_cos_deg, fr_tan_deg\n"
        "\n"
        "  Inverse trig (input: value):\n"
        "    FR_acos, FR_asin, FR_atan\n"
        "\n"
        "  Inverse trig (input: degrees on unit circle):\n"
        "    FR_atan2\n"
        "\n"
        "  Logarithmic (input: value):\n"
        "    FR_log2, FR_ln, FR_log10\n"
        "\n"
        "  Exponential (input: exponent):\n"
        "    FR_pow2, FR_EXP, FR_POW10\n"
        "\n"
        "  Other:\n"
        "    FR_sqrt (input: value)\n"
        "    FR_hypot, FR_hypot_fast8 (input: x, --y for y)\n"
        "\n"
        "  center:  center of sweep (degrees for trig/atan2, value otherwise)\n"
        "  half:    number of samples each side of center\n"
        "\n"
        "Options:\n"
        "  --inc <step>         increment (default depends on function)\n"
        "  --fmt md|csv|ascii   output format (default: md)\n"
        "  --radix <r>          input radix for fixed-point (default: 16)\n"
        "  --out_radix <r>      output radix for inv trig/log (default: 16)\n"
        "  --y <val>            fixed y value for hypot functions (default: 0.0)\n"
        "\n"
        "Examples:\n"
        "  trig_neighborhood fr_cos -90 15\n"
        "  trig_neighborhood fr_sin -360 10 --fmt csv\n"
        "  trig_neighborhood fr_tan 89.5 20 --inc 0.01\n"
        "  trig_neighborhood fr_sin_deg 45 10 --radix 8\n"
        "  trig_neighborhood FR_asin 0.5 15 --radix 15 --out_radix 16\n"
        "  trig_neighborhood FR_log2 1.0 15 --inc 0.01\n"
        "  trig_neighborhood FR_atan2 90 15\n"
        "  trig_neighborhood FR_hypot_fast8 100 15 --y 50 --radix 8\n"
    );
}

int main(int argc, char **argv) {
    if (argc < 4) { usage(); return 1; }

    Func func = parse_func(argv[1]);
    if (func == F_UNKNOWN) {
        fprintf(stderr, "Unknown function: %s\n", argv[1]);
        usage();
        return 1;
    }

    double center = atof(argv[2]);
    int half = atoi(argv[3]);
    double inc = -1.0;  /* sentinel: use default */
    Fmt fmt = FMT_MD;
    int radix = 16;
    int out_radix = 16;
    double y_val = 0.0;

    for (int i = 4; i < argc; i++) {
        if (!strcmp(argv[i], "--inc") && i + 1 < argc)
            inc = atof(argv[++i]);
        else if (!strcmp(argv[i], "--fmt") && i + 1 < argc) {
            i++;
            if (!strcmp(argv[i], "csv"))   fmt = FMT_CSV;
            else if (!strcmp(argv[i], "ascii")) fmt = FMT_ASCII;
            else fmt = FMT_MD;
        }
        else if (!strcmp(argv[i], "--radix") && i + 1 < argc)
            radix = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--out_radix") && i + 1 < argc)
            out_radix = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--y") && i + 1 < argc)
            y_val = atof(argv[++i]);
    }

    if (inc < 0.0) inc = default_inc(func);

    const char *cols[] = {"sample", "val", "input_fp", "radix", "raw_got", "raw_exp", "expected", "got", "abs_err", "pct_err"};
    int ncols = 10;

    switch (fmt) {
    case FMT_CSV:
        for (int c = 0; c < ncols; c++)
            printf("%s%s", cols[c], c < ncols - 1 ? "," : "\n");
        break;
    case FMT_MD:
        printf("**%s** center=%.6f, +/-%d samples, inc=%.6g, radix=%d",
               func_name(func), center, half, inc, radix);
        if (out_radix != radix)
            printf(", out_radix=%d", out_radix);
        if (func == F_HYPOT || func == F_HYPOT_FAST8)
            printf(", y=%.6f", y_val);
        printf("\n\n");
        printf("|");
        for (int c = 0; c < ncols; c++) printf(" %s |", cols[c]);
        printf("\n|");
        for (int c = 0; c < ncols; c++) printf("---|");
        printf("\n");
        break;
    case FMT_ASCII:
        printf("# %s  center=%.6f  +/-%d  inc=%.6g  radix=%d",
               func_name(func), center, half, inc, radix);
        if (out_radix != radix)
            printf("  out_radix=%d", out_radix);
        if (func == F_HYPOT || func == F_HYPOT_FAST8)
            printf("  y=%.6f", y_val);
        printf("\n");
        printf("%8s %12s %12s %6s %10s %10s %12s %12s %12s %12s\n",
               cols[0], cols[1], cols[2], cols[3], cols[4], cols[5], cols[6], cols[7], cols[8], cols[9]);
        printf("%8s %12s %12s %6s %10s %10s %12s %12s %12s %12s\n",
               "--------", "------------", "------------", "------",
               "----------", "----------",
               "------------", "------------", "------------", "------------");
        break;
    }

    for (int k = -half; k <= half; k++) {
        double val = center + k * inc;
        s32 input_fp;
        double expected;
        int out_prec;
        s32 raw = eval(func, val, radix, out_radix, y_val, &input_fp, &expected, &out_prec);
        s32 raw_exp = (s32)floor(ldexp(expected, out_prec) + 0.5);
        double got = frd(raw, out_prec);
        double ae = fabs(got - expected);
        double pe = (expected != 0.0) ? ae / fabs(expected) * 100.0 : (ae != 0.0 ? 100.0 : 0.0);

        switch (fmt) {
        case FMT_CSV:
            printf("%d,%.6g,%d,%d,%d,%d,%.6f,%.6f,%.6f,%.4f%%\n",
                   k, val, input_fp, radix, raw, raw_exp, expected, got, ae, pe);
            break;
        case FMT_MD:
            printf("| %d | %.6g | %d | %d | %d | %d | %.6f | %.6f | %.6f | %.4f%% |\n",
                   k, val, input_fp, radix, raw, raw_exp, expected, got, ae, pe);
            break;
        case FMT_ASCII:
            printf("%8d %12.6g %12d %6d %10d %10d %12.6f %12.6f %12.6f %11.4f%%\n",
                   k, val, input_fp, radix, raw, raw_exp, expected, got, ae, pe);
            break;
        }
    }

    return 0;
}
