/**
 *  @file FR_tan32.c - division-free tangent and binary-search atan2
 *
 *  fr_tan_bam32:  hybrid tangent — table lookup + sin/cos near pole.
 *      0-45°:  direct u32 lerp from gFR_TAN_TAB_Q[0..64].
 *      45-75°: variable-radix u16 mantissa + shift tables (no division).
 *      75-90°: sin/cos ratio from cosine table (one s64 division).
 *
 *  fr_tan_bam32_d64: full-range sin/cos ratio from cosine table.
 *                    Kept for comparison.  One s64 division per call.
 *
 *  fr_atan2_32:   binary search on the 129-entry u32 tan quadrant table
 *                 (gFR_TAN_TAB_Q), then quadrant mapping.
 *
 *  @copy Copyright (C) <2001-2026>  <M. A. Chatterjee>
 *  @author M A Chatterjee <deftio [at] deftio [dot] com>
 *
 */

#include "FR_math.h"
#include "FR_trig_table.h"
#include "FR_tan_table.h"

#ifndef FR_NO_STDINT
#include <stdint.h>
#endif

/*=======================================================
 * cos_lerp_full — interpolated cosine from the 129-entry quadrant table.
 *
 * Returns cos(inq) in high-precision fixed-point (7 extra frac bits).
 * Used internally by fr_tan_bam32 for the 75°-90° sin/cos path and
 * by fr_tan_bam32_d64 for the full-range sin/cos path.
 */
static s32 cos_lerp_full(u32 inq)
{
    u32 idx  = inq >> FR_TRIG_FRAC_BITS;
    u32 frac = inq &  FR_TRIG_FRAC_MASK;
    s32 lo   = gFR_COS_TAB_Q[idx];
    s32 d    = lo - gFR_COS_TAB_Q[idx + 1];
    return (lo << FR_TRIG_FRAC_BITS) - d * (s32)frac;
}

/*=======================================================
 * fr_tan_bam32 — hybrid tangent: table lookup + sin/cos near pole.
 *
 * Three zones:
 *   0°-45°:  direct u32 lerp from gFR_TAN_TAB_Q[0..64].
 *            7-bit index + 7-bit frac.  All u32, no division.
 *
 *   45°-75°: variable-radix u16 mantissa + u8 shift tables
 *            (gFR_TAN_MANT_Q2 / gFR_TAN_SHIFT_Q2).
 *            All u32, no division.
 *
 *   75°-90°: sin/cos ratio via the 129-entry cosine table.
 *            One s64 division.  Handles the pole accurately.
 *
 * Poles: ±FR_TRIG_MAXVAL (90° = +, 270° = -).
 * Result: s32 at radix 16 (s15.16).
 */
#define FR_TAN_OCT_HALF   (1 << 13)   /* 8192 = 45 deg in BAM quadrant */
#define FR_TAN_D64_THRESH ((u32)(75.0 / 90.0 * 16384 + 0.5))  /* 13653 */

s32 fr_tan_bam32(u16 bam)
{
    u32 q    = ((u32)bam >> 14) & 0x3;
    u32 inq  = (u32)bam & 0x3FFFu;
    s32 sign = (q & 1) ? -1 : 1;

    /* Poles: exactly 90° or 270° */
    if (inq == 0 && (q & 1))
        return (q == 1) ? FR_TRIG_MAXVAL : -FR_TRIG_MAXVAL;

    if (q & 1)
        inq = 0x4000u - inq;

    u32 raw;

    if (inq <= FR_TAN_OCT_HALF) {
        /* First octant (0°-45°): direct u32 table lookup */
        u32 idx   = inq >> FR_TAN32_FRAC_BITS;
        u32 frac  = inq &  FR_TAN32_FRAC_MASK;
        u32 lo    = gFR_TAN_TAB_Q[idx];
        u32 delta = gFR_TAN_TAB_Q[idx + 1] - lo;
        raw = lo + ((delta * frac) >> FR_TAN32_FRAC_BITS);
    } else if (inq < FR_TAN_D64_THRESH) {
        /* Second octant 45°-75°: variable-radix u16+shift */
        u32 oct2  = inq - FR_TAN_OCT_HALF;
        u32 idx   = oct2 >> FR_TAN32_FRAC_BITS;
        u32 frac  = oct2 &  FR_TAN32_FRAC_MASK;

        u32 m_lo  = gFR_TAN_MANT_Q2[idx];
        u32 m_hi  = gFR_TAN_MANT_Q2[idx + 1];
        u32 s_lo  = gFR_TAN_SHIFT_Q2[idx];
        u32 s_hi  = gFR_TAN_SHIFT_Q2[idx + 1];
        u32 s_max = (s_hi > s_lo) ? s_hi : s_lo;

        u32 a_lo  = m_lo >> (s_max - s_lo);
        u32 a_hi  = m_hi >> (s_max - s_hi);
        u32 delta = a_hi - a_lo;

        raw = (a_lo + ((delta * frac) >> FR_TAN32_FRAC_BITS)) << s_max;
    } else {
        /* 75°-90°: sin/cos ratio from cosine table (one s64 division) */
        s32 cos_val = cos_lerp_full(inq);
        s32 sin_val = cos_lerp_full(FR_TAN32_QUADRANT - inq);
        if (cos_val == 0)
            raw = (u32)FR_TRIG_MAXVAL;
        else
            raw = (u32)((((s64)sin_val << 16) + ((s64)cos_val >> 1)) / (s64)cos_val);
    }

    return (sign < 0) ? -(s32)raw : (s32)raw;
}

/*=======================================================
 * fr_tan_bam32_d64 — tangent via sin/cos from the cosine table.
 *
 * Full-range sin/cos implementation kept for comparison.
 * Computes sin(x)/cos(x) using the 129-entry cosine quadrant table.
 * One s64 division per call.
 */
s32 fr_tan_bam32_d64(u16 bam)
{
    u32 q   = ((u32)bam >> 14) & 0x3;
    u32 inq = (u32)bam & 0x3FFFu;
    s32 sign = 1;
    s32 sin_val, cos_val;
    s32 raw;

    if (inq == 0 && (q == 0 || q == 2))
        return 0;
    if (inq == 0 && (q == 1 || q == 3))
        return (q == 1) ? FR_TRIG_MAXVAL : -FR_TRIG_MAXVAL;

    if (q == 1 || q == 3) {
        inq = 0x4000u - inq;
        sign = -1;
    }

    cos_val = cos_lerp_full(inq);
    sin_val = cos_lerp_full(FR_TAN32_QUADRANT - inq);

    if (cos_val == 0)
        raw = FR_TRIG_MAXVAL;
    else {
        raw = (s32)((((s64)sin_val << 16) + ((s64)cos_val >> 1)) / (s64)cos_val);
    }

    return (sign < 0) ? -raw : raw;
}

/* fr_tan32: tan from radians at caller-specified radix. s15.16 result. */
s32 fr_tan32(s32 rad, u16 radix)
{
    return fr_tan_bam32(fr_rad_to_bam(rad, radix));
}

/* fr_tan_deg32: tan from degrees at caller-specified radix. s15.16 result.
 * radix 0 = integer degrees, radix > 0 = fixed-point degrees with that
 * many fractional bits.  s32 input so e.g. radix=16 gives s15.16 degrees. */
s32 fr_tan_deg32(s32 deg, u16 radix)
{
    u16 bam = (radix == 0) ? FR_DEG2BAM_I((s16)deg)
            : fr_deg_to_bam(deg, radix);
    return fr_tan_bam32(bam);
}

/*=======================================================
 * fr_atan_bam32 - Arctangent via binary search on the tan table.
 *
 * Input:  positive ratio in s15.16 (caller handles signs/quadrants).
 * Output: BAM angle (u16) in [0, 0x4000) representing [0, 90 deg).
 *
 * Algorithm:
 *   1. If x <= 0: return 0.
 *   2. If x >= table[127]: return near-pole BAM (saturate ~89.3 deg).
 *   3. Binary search: 7 iterations on 128 entries to bracket.
 *   4. Linear interpolation within bracket for 7 fractional bits.
 *   5. Assemble: bam = (idx << 7) | frac.
 */
static u16 fr_atan_bam32(s32 x)
{
    s32 lo, hi, mid;
    s32 idx, d, num, frac;
    u32 ux;

    if (x <= 0)
        return 0;

    ux = (u32)x;

    /* Saturate near the pole */
    if (ux >= gFR_TAN_TAB_Q[127])
        return (u16)((127u << FR_TAN32_FRAC_BITS) + FR_TAN32_FRAC_MASK);

    /* Binary search: find lo such that table[lo] <= ux < table[lo+1].
     * The table is monotonically increasing. */
    lo = 0;
    hi = 127;
    while (lo < hi) {
        mid = (lo + hi + 1) >> 1;
        if (gFR_TAN_TAB_Q[mid] <= ux)
            lo = mid;
        else
            hi = mid - 1;
    }

    /* lo is now the index where table[lo] <= ux < table[lo+1]. */
    idx = lo;

    /* Linear interpolation within the bracket */
    d   = (s32)(gFR_TAN_TAB_Q[idx + 1] - gFR_TAN_TAB_Q[idx]);
    num = (s32)(ux - gFR_TAN_TAB_Q[idx]);
    if (d > 0)
        frac = (s32)(((s64)num << FR_TAN32_FRAC_BITS) / d);
    else
        frac = 0;

    if (frac > FR_TAN32_FRAC_MASK)
        frac = FR_TAN32_FRAC_MASK;

    return (u16)(((u32)idx << FR_TAN32_FRAC_BITS) + (u32)frac);
}

/*=======================================================
 * fr_atan2_32 - Full-circle atan2 using the tan table binary search.
 *
 * Input:  y, x as s32 values at radix 16 (s15.16).
 * Output: radians at out_radix.
 * Range:  [-pi, pi].  Returns 0 for atan2(0, 0).
 *
 * Algorithm:
 *   1. Handle axis cases.
 *   2. Compute ratio = |y| / |x| or |x| / |y| (whichever <= 1.0) in s15.16.
 *   3. Binary search -> BAM angle in [0, pi/4].
 *   4. If |y| > |x|: angle = pi/2 - angle.
 *   5. Apply quadrant from signs of x and y.
 */
s32 fr_atan2_32(s32 y, s32 x, u16 out_radix)
{
    s32 ax, ay, ratio;
    u16 bam;
    s32 angle;
    s32 pi, half_pi;

    pi      = FR_CHRDX(FR_kPI, FR_kPREC, out_radix);
    half_pi = FR_CHRDX(FR_kQ2RAD, FR_kPREC, out_radix);

    /* Axis cases */
    if (x == 0) {
        if (y > 0) return  half_pi;
        if (y < 0) return -half_pi;
        return 0;
    }
    if (y == 0)
        return (x > 0) ? 0 : pi;

    ax = (x < 0) ? -x : x;
    ay = (y < 0) ? -y : y;

    /* Compute ratio in s15.16. Use the smaller/larger to stay in [0, 1.0]
     * for the initial lookup, then complement if needed. */
    if (ay <= ax) {
        /* angle in [0, 45 deg]: ratio = ay/ax */
        ratio = (s32)(((s64)ay << 16) / ax);
        bam = fr_atan_bam32(ratio);
        /* Convert BAM to radians at out_radix */
        angle = FR_CHRDX(FR_Q2RAD(bam), 14, out_radix);
    } else {
        /* angle in (45, 90 deg): ratio = ax/ay, angle = pi/2 - atan(ratio) */
        ratio = (s32)(((s64)ax << 16) / ay);
        bam = fr_atan_bam32(ratio);
        angle = half_pi - FR_CHRDX(FR_Q2RAD(bam), 14, out_radix);
    }

    /* Apply quadrant from signs of x and y */
    if (x > 0)
        return (y > 0) ? angle : -angle;
    else
        return (y > 0) ? (pi - angle) : (angle - pi);
}
