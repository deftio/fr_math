/**
 *	@FR_math.h - header definition file for fixed radix math routines
 *
 *	@copy Copyright (C) <2001-2026>  <M. A. Chatterjee>
 *  @author M A Chatterjee <deftio [at] deftio [dot] com>
 *
 *  This file contains integer math settable fixed point radix math routines for
 *  use on systems in which floating point is not desired or unavailable.
 *
 *  @license:
 *	This software is provided 'as-is', without any express or implied
 *	warranty. In no event will the authors be held liable for any damages
 *	arising from the use of this software.
 *
 *	Permission is granted to anyone to use this software for any purpose,
 *	including commercial applications, and to alter it and redistribute it
 *	freely, subject to the following restrictions:
 *
 *	1. The origin of this software must not be misrepresented; you must not
 *	claim that you wrote the original software. If you use this software
 *	in a product, an acknowledgment in the product documentation would be
 *	appreciated but is not required.
 *
 *	2. Altered source versions must be plainly marked as such, and must not be
 *	misrepresented as being the original software.
 *
 *	3. This notice may not be removed or altered from any source
 *	distribution.
 *
 */

#ifndef __FR_Math_h__
#define __FR_Math_h__

#define FR_MATH_VERSION     "2.0.7"
#define FR_MATH_VERSION_HEX  0x020007  /* major << 16 | minor << 8 | patch */

#ifdef FR_CORE_ONLY
#define FR_NO_PRINT
#define FR_NO_WAVES
#endif

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef __FR_Platform_Defs_H__
#include "FR_defs.h"
#endif

/* Quick Note on MACRO param wrapping:
 * All macro inputs are wrapped in paranthesis in this code.
 * eg: #define MACRO_X_SQUARED(x)  ((x)*(x)) //<<-- note internal paranthesis
 * this is done because macros use true source substitution in C/C++ so a if
 * a macro internally uses many operators of mixed precedence e.g. >> and * together
 * undesired behavior can result if the parameter "passed" in the the macro is a
 * a complex contruct e.g. instead of being a value or single variable is a
 * something like 3+4*5  --> all of this would gets substituted in to the MACRO
 * expression and parans eliminate chances for odd behavior.
 * For example:
 * MACRO_X_SQUARED_BAD(x) (x*x)
 * will expand this way:
 * 3+4*5*3+4*5 ==> 3+60+20 == 83 // due to precedence operations whereas
 * MACRO_X_SQUARED(x) ((x)*(x))
 * (3+4*5)*(3+4*5) ==> (3+20)*(3+20) == (23)*(23) == 529
 */

/*absolute value for integer and fixed radix types*/
#define FR_ABS(x) (((x) < 0) ? (-(x)) : (x))

/*sign of x.  Not as good as The Sign of Four, but I digress           */
#define FR_SGN(x) ((x) >> ((((signed)sizeof(x)) << 3) - 1))

/*===============================================
 * Simple Fixed Point Math Conversions
 * r is radix precision in bits, converts to/from integer w truncation */
#define I2FR(x, r) ((x) << (r))
#define FR2I(x, r) ((x) >> (r))

/*===============================================
 * Make a fixed radix number from integer + base-10 fractional parts.
 * r  = output radix (fractional bits)
 * i  = integer part (signed)
 * f  = decimal-fraction digits as written, e.g. for "12.34" pass f=34
 * d  = number of decimal digits in f, e.g. for "12.34" pass d=2
 *
 * FR_NUM(12, 34, 2, 10)  ≈ 12.34 in s.10  = (12<<10) + (34<<10)/100 = 12636
 * FR_NUM(-3, 5,  1, 16)  ≈ -3.5  in s.16  = (-3<<16) - (5<<16)/10
 *
 * The fraction is rounded toward zero. For round-to-nearest, add half an LSB
 * before scaling at the call site. Sign of the fractional part follows the
 * sign of i (for i==0 the result is positive, matching "+0.5" intuition).
 */
#define FR_NUM_POW10(d) (                                              \
    ((d) == 0) ? 1L :                                                  \
    ((d) == 1) ? 10L :                                                 \
    ((d) == 2) ? 100L :                                                \
    ((d) == 3) ? 1000L :                                               \
    ((d) == 4) ? 10000L :                                              \
    ((d) == 5) ? 100000L :                                             \
    ((d) == 6) ? 1000000L :                                            \
    ((d) == 7) ? 10000000L :                                           \
    ((d) == 8) ? 100000000L : 1000000000L)
#define FR_NUM(i, f, d, r) (                                           \
    ((s32)(i) << (r)) +                                                \
    (((i) < 0)                                                         \
        ? -((s32)(((s32)(f) << (r)) / FR_NUM_POW10(d)))                \
        :  ((s32)(((s32)(f) << (r)) / FR_NUM_POW10(d)))))
/*
FR_INT(x,r) convert a fixed radix variable x of radix r to an integer
*/
#define FR_INT(x, r) (((x) < 0) ? -((-(x)) >> (r)) : ((x) >> (r)))

/* Change Radix (x,current_radix, new_radix)
 * change number from its current fixed radix (can be 0 or integer) to a new fixed radix
 * Useful when dealing with numbers with mixed radixes.  This is a MACRO so
 * this code expands in place and x is modified.
 */
#define FR_CHRDX(x, r_cur, r_new) (((r_cur) - (r_new)) >= 0 ? ((x) >> ((r_cur) - (r_new))) : ((x) << ((r_new) - (r_cur))))

/* return only the fractional part of x */
#define FR_FRAC(x, r) ((FR_ABS(x)) & (((1 << (r)) - 1)))

/* return the fractional part of number x with radix xr scaled to radix nr bits */
#define FR_FRACS(x, xr, nr) (FR_CHRDX(FR_FRAC((x), (xr)), (xr), (nr)))

/******************************************************
  Add (sub) to fixed point numbers by converting the second number to the
  same radix as the first. If yr < xr then possibility of overflow is increased.
  Note: for two vars, i, j, of prec ir, jr, it is not necessarily true that
  FR_ADD(i,ir,j,jr) == FR_ADD(j,jr,i,ir)
*/
#define FR_ADD(x, xr, y, yr) ((x) += FR_CHRDX(y, yr, xr))
#define FR_SUB(x, xr, y, yr) ((x) -= FR_CHRDX(y, yr, xr))

/* Fixed-radix division with round-to-nearest: x (at radix xr) / y (at radix yr),
 * result at radix xr. Uses a 64-bit intermediate so the full Q16.16 range works
 * correctly. Adds half the divisor before truncation to achieve ≤ 0.5 LSB error. */
static inline s32 FR_div_rnd(s64 num, s32 den) {
    if ((num ^ den) >= 0)                   /* same sign: positive quotient */
        return (s32)((num + den / 2) / den);
    else                                     /* negative quotient */
        return (s32)((num - den / 2) / den);
}
#define FR_DIV(x, xr, y, yr) FR_div_rnd((s64)(x) << (yr), (s32)(y))

/* FR_DIV_TRUNC: truncating division (old FR_DIV behavior). Useful when the
 * caller knows the quotient is exact or truncation is acceptable. */
#define FR_DIV_TRUNC(x, xr, y, yr) ((s32)(((s64)(x) << (yr)) / (s32)(y)))

/* FR_DIV32: 32-bit-only division. Requires |x| < 2^(31-yr) to avoid
 * overflow in the intermediate (x << yr). Use FR_DIV for full-range
 * division with 64-bit intermediate. */
#define FR_DIV32(x, xr, y, yr) (((s32)(x) << (yr)) / (s32)(y))

/* Remainder: both operands should be at the same radix. */
#define FR_MOD(x, y) ((x) % (y))

/* min, max, clamp */
#define FR_MIN(a, b)         (((a) < (b)) ? (a) : (b))
#define FR_MAX(a, b)         (((a) > (b)) ? (a) : (b))
#define FR_CLAMP(x, lo, hi)  (FR_MIN(FR_MAX((x), (lo)), (hi)))

/* Check if x is a power of 2. */
#define FR_ISPOW2(x) (!((x) & ((x) - 1)))

/* floor and ceiling in current radix, leaving current radix intact
 * this means the lower radix number of bits are set to 0
 */
#define FR_FLOOR(x, r) ((x) & (~((1 << r) - 1)))
#define FR_CEIL(x, r) (FR_FLOOR(x, r) + ((FR_FRAC(x, r) ? (1 << r) : 0)))

/*******************************************************
  Interpolate between 2 fixed point values (x0,x1) of any radix,
  with a fractional delta, of a supplied precision.
  If delta is outside 0<= delta<=1 then extrapolation
  does work but programmer should watch for possible overflow.
  x0,x1 need not have same radix as delta
*/
#define FR_INTERP(x0, x1, delta, prec) ((x0) + ((((x1) - (x0)) * (delta)) >> (prec)))

/******************************************************
   FR_INTERPI is the same as FR_INTERP except that  insures the
   range is btw [0<= delta < 1] --> ((0 ... (1<<prec)-1)
   Note: delta should be >= 0
*/
#define FR_INTERPI(x0, x1, delta, prec) ((x0) + ((((x1) - (x0)) * ((delta) & ((1 << (prec)) - 1))) >> (prec)))

/******************************************************
  Convert to double, this is for debug only and WILL NOT compile under many embedded systems.
  Fixed Radix to Floating Point Double Conversion
  since this is a MACRO it will not be compiled or instantiated unless it is actually called in code.
*/
#define FR2D(x, r) ((double)(((double)(x)) / ((double)(1 << (r)))))
#define D2FR(d, r) ((s32)(d * (1 << r)))

/******************************************************
  Useful Constants

  FR_kXXXX "k" denotes constant to help when reading macros used in source code
  FR_krXXX "r" == reciprocal e.g. 1/XXX
  As these are MACROS, they take only compiled code space if actually used.
  Consts here calculated by multiply the natural base10 value by (1<<FR_kPREC)
 */
#define FR_kPREC (16)         /* bits of precision in constants listed below */
#define FR_kE (178145)        /* 2.718281828459 */
#define FR_krE (24109)        /* 0.367879441171 */
#define FR_kPI (205887)       /* 3.141592653589 */
#define FR_krPI (20861)       /* 0.318309886183 */
#define FR_kDEG2RAD (1144)    /* 0.017453292519 */
#define FR_kRAD2DEG (3754936) /*57.295779513082 */

#define FR_kQ2RAD (102944) /* 1.570796326794 */
#define FR_kRAD2Q (41722)  /* 0.636619772367 */

/*log2 to ln conversions (see MACROS) */
#define FR_kLOG2E (94548)  /* 1.442695040890 */
#define FR_krLOG2E (45426) /* 0.693147180560 */

/*log2 to log10 conversions (see MACROS) */
#define FR_kLOG2_10 (217706) /* 3.32192809489 */
#define FR_krLOG2_10 (19728) /* 0.30102999566 */

/* High-precision scaling constants at radix 28.
 * Used by FR_EXP, FR_ln, FR_log10 for base conversion.
 * At radix 28 these have ~9 decimal digits of precision, far exceeding
 * the ~4.8 digits of Q16.16.
 */
#define FR_kLOG2E_28    (387270501)   /* log2(e)   = 1.4426950408889634  */
#define FR_krLOG2E_28   (186065279)   /* ln(2)     = 0.6931471805599453  */
#define FR_kLOG2_10_28  (891723283)   /* log2(10)  = 3.3219280948873622  */
#define FR_krLOG2_10_28  (80807124)   /* log10(2)  = 0.3010299956639812  */

/* Multiply fixed-point value x (any radix) by a radix-28 constant k.
 * Result stays at x's radix. Uses 64-bit intermediate.
 * Rounds to nearest (adds 0.5 LSB before shift).
 */
#define FR_MULK28(x, k) ((s32)((((int64_t)(x) * (int64_t)(k)) + (1 << 27)) >> 28))

// common sqrts
#define FR_kSQRT2 (92682)   /* 1.414213562373 */
#define FR_krSQRT2 (46341)  /* 0.707106781186 */
#define FR_kSQRT3 (113512)  /* 1.732050807568 */
#define FR_krSQRT3 (37837)  /* 0.577350269189 */
#define FR_kSQRT5 (146543)  /* 2.236067977599 */
#define FR_krSQRT5 (29309)  /* 0.447213595499 */
#define FR_kSQRT10 (207243) /* 3.162277660168 */
#define FR_krSQRT10 (20724) /* 0.316227766016 */

  /*===============================================
   * Arithmetic operations
   */
  s32 FR_FixMuls(s32 x, s32 y);   // mul signed, round-to-nearest, NOT saturated
  s32 FR_FixMulSat(s32 x, s32 y); // mul signed, round-to-nearest, saturated
  s32 FR_FixAddSat(s32 x, s32 y); // add signed, saturated

/*================================================
 * Constants used in Trig tables, definitions
 *
 * FR_TRIG_PREC     — internal table precision (s0.15, kept for table indexing)
 * FR_TRIG_OUT_PREC — output precision of sin/cos/tan (s15.16 since v2.0.1)
 * FR_TRIG_ONE      — exact 1.0 in output format (1 << 16 = 65536)
 *
 * sin/cos return s32 at radix 16 (s15.16). This matches libfixmath Q16.16
 * precision and allows exact representation of 1.0 at the poles.
 * tan returns s32 at radix 16 (s15.16). Saturates at ±FR_TRIG_MAXVAL.
 */
#define FR_TRIG_PREC     (15)
#define FR_TRIG_OUT_PREC (16)
#define FR_TRIG_MASK     ((1 << (FR_TRIG_PREC)) - 1)
#define FR_TRIG_ONE      (1L << FR_TRIG_OUT_PREC)         /* 65536 = 1.0 */
#define FR_TRIG_MAXVAL   ((s32)0x7fffffff)                 /* tan saturation max */
#define FR_TRIG_MINVAL   (-FR_TRIG_MAXVAL)                  /* tan saturation min */

/* Bit Shift Scaling macros.  Useful on some platforms with poor MUL performance.
 * Also can be useful if you need to scale numbers with
 * large portions of bits_in_use and a larger register size is not available.
 * For example, suppose you need to scale a large 32bit number say z=0xa4239323
 * from degrees to radians.  Ideally this number would be multiplied by 0.017
 * but using a (z*FR_kDEG2RAD) >> FR_kPREC type operation is likely to overflow
 * due the MUL result being large.  The FR_RAD2DEG MACRO below doesn't require
 * accumulator headroom bits and so has no chance for loss of precision.
 * Another benefit is  some low end micros (8051, 68xx, MSP430(low end),
 * PIC-8 family) have no multiplier.  So these allow the programmer an option
 * to see if performance or precision are better expressed as shifts rather than
 * scaled multiplies.  As always, mileage may vary depending on architecture,
 * compiler and other considerations.
 */

/* scale by 10s */
#define FR_SMUL10(x) (((x) << 3) + (((x) << 1)))
#define FR_SDIV10(x) (((x) >> 3) - ((x) >> 5) + ((x) >> 7) - ((x) >> 9) + ((x) >> 11))

/* scale by 1/log2(e)  0.693147180560 used for converting log2() to ln()  */
#define FR_SrLOG2E(x) (((x) >> 1) + ((x) >> 2) - ((x) >> 3) + ((x) >> 4) + ((x) >> 7) - ((x) >> 9) - ((x) >> 12) + ((x) >> 15))

/* scale by log2(e)    1.442695040889 used for converting pow2() to exp() */
#define FR_SLOG2E(x) ((x) + ((x) >> 1) - ((x) >> 4) + ((x) >> 8) + ((x) >> 10) + ((x) >> 12) + ((x) >> 14))

/* scale by 1/log2(10) 0.30102999566 used for converting log2() to log10  */
#define FR_SrLOG2_10(x) (((x) >> 2) + ((x) >> 4) - ((x) >> 6) + ((x) >> 7) - ((x) >> 8) + ((x) >> 12))

/* scale by log2(10)   3.32192809489 used for converting pow2() to pow10 */
#define FR_SLOG2_10(x) (((x) << 1) + (x) + ((x) >> 2) + ((x) >> 4) + ((x) >> 7) + ((x) >> 10) + ((x) >> 11) + ((x) >> 13))

/* Shift-only angular conversion macros
 *
 * All are pure constant multipliers expressed as shifts — no multiply, no
 * divide, no 64-bit intermediates, no accumulators. Work at any radix: if
 * your input is degrees at radix 8, the output is the target unit at radix 8.
 * The caller shifts as needed.
 *
 * Angular units:
 *   degrees   = 360  per revolution
 *   radians   = 2*pi per revolution
 *   BAM       = 65536 per revolution (Binary Angular Measure, u16)
 *   quadrants = 4 per revolution (= BAM >> 14)
 *
 * Side-effect note: x is referenced multiple times in each macro — do not
 * pass expressions with side effects.
 */

/* FR_DEG2RAD(x): multiply by pi/180 ≈ 0.017453 (5 terms, ~17 bits) */
#define FR_DEG2RAD(x) (((x) >> 6) + ((x) >> 9) - ((x) >> 13) - ((x) >> 19) - ((x) >> 20))

/* FR_RAD2DEG(x): multiply by 180/pi ≈ 57.29578 (7 terms, ~19 bits) */
#define FR_RAD2DEG(x) (((x) << 6) - ((x) << 3) + (x) + ((x) >> 2) + (((x) >> 4) - ((x) >> 6)) - ((x) >> 10))

/* FR_DEG2BAM(x): multiply by 65536/360 ≈ 182.0449 (7 terms, ~18 bits).
 * CAUTION: overflows s32 when |x| > ~256 deg at s15.16 (x<<7 term).
 * For safe conversion at any radix, use fr_deg_to_bam() instead. */
#define FR_DEG2BAM(x) (((x)<<7)+((x)<<6)-((x)<<3)-((x)<<1)+((x)>>5)+((x)>>6)-((x)>>9))

/* FR_BAM2DEG(x): multiply by 360/65536 = 0.00549316 (4 terms, exact) */
#define FR_BAM2DEG(x) (((x)>>8)+((x)>>9)-((x)>>12)-((x)>>13))

/* FR_RAD2BAM(x): multiply by 65536/(2*pi) ≈ 10430.378 (7 terms, ~21 bits).
 * CAUTION: overflows s32 when |x| > ~4 rad at s15.16 (x<<13 term).
 * For safe conversion at any radix, use fr_rad_to_bam() instead. */
#define FR_RAD2BAM(x) (((x)<<13)+((x)<<11)+((x)<<7)+((x)<<6)-((x)<<1)+((x)>>1)-((x)>>3))

/* ── Overflow-safe rad/deg to BAM conversion functions ─────────────
 *
 * These replace the FR_RAD2BAM / FR_DEG2BAM macros for callers that
 * need the full ±2*pi or ±360° range at any radix.
 *
 * Strategy: normalize input to radix 16, conditionally reduce into
 * a safe zone, apply the full-precision shift-only multiply, then
 * extract the u16 BAM.  No precision loss from halving/quartering.
 *
 * fr_rad_to_bam: reduce to [-pi, pi], reordered terms.  ±2*pi safe.
 * fr_deg_to_bam: reduce to [-90, 90) + quadrant offset.  ±360° safe.
 */

/* Constants at radix 16 */
#define FR_PI_R16       205887   /* round(pi * 65536) */
#define FR_TWO_PI_R16   411775   /* round(2*pi * 65536) */
#define FR_D90_R16      5898240  /* 90 * 65536 */
#define FR_D180_R16     11796480 /* 180 * 65536 */
#define FR_D360_R16     23592960 /* 360 * 65536 */

static u16 __attribute__((unused)) fr_rad_to_bam(s32 rad, u16 radix)
{
    /* Normalize to radix 16 */
    s32 r = (radix > 16) ? (rad >> (radix - 16))
          : (radix < 16) ? (rad << (16 - radix))
          : rad;

    /* Reduce to [-pi, pi] — one conditional pass, covers ±2*pi input */
    if (r >  FR_PI_R16) r -= FR_TWO_PI_R16;
    if (r < -FR_PI_R16) r += FR_TWO_PI_R16;

    /* Shift terms reordered: interleave negatives early to keep all
     * intermediate sums within s32.  Same 7-term decomposition as
     * FR_RAD2BAM, just reordered.  Safe for |r| <= 205887 (pi). */
    s32 bam = (r<<13)-(r<<1)+(r<<11)-(r>>3)+(r<<7)+(r<<6)+(r>>1);
    return (u16)((bam + (1 << 15)) >> 16);
}

static u16 __attribute__((unused)) fr_deg_to_bam(s32 deg, u16 radix)
{
    /* Normalize to radix 16 */
    s32 d = (radix > 16) ? (deg >> (radix - 16))
          : (radix < 16) ? (deg << (16 - radix))
          : deg;

    /* Reduce to [-180, 180) — covers ±360 input */
    if (d >=  FR_D180_R16) d -= FR_D360_R16;
    if (d <  -FR_D180_R16) d += FR_D360_R16;

    /* Reduce to [-90, 90) with BAM quadrant offset.
     * Needed because 182 * 11796480 (±180° at r16) overflows s32. */
    u16 offset = 0;
    if (d >= FR_D90_R16)      { d -= FR_D180_R16; offset = 32768; }
    else if (d < -FR_D90_R16) { d += FR_D180_R16; offset = 32768; }

    /* |d| < 90° at r16. Max intermediate = 5898240 * 192 = 1.13B, safe. */
    s32 bam = (d<<7)+(d<<6)-(d<<3)-(d<<1)+(d>>5)+(d>>6)-(d>>9);
    return (u16)(offset + (u16)((bam + (1 << 15)) >> 16));
}

/* FR_BAM2RAD(x): multiply by 2*pi/65536 ≈ 0.0000959 (5 terms, ~18 bits) */
#define FR_BAM2RAD(x) (((x)>>13)-((x)>>15)+((x)>>18)+((x)>>21)+((x)>>25))

/* Legacy quadrant macros (quadrants = BAM >> 14) */
#define FR_RAD2Q(x) (((x) >> 1) + ((x) >> 3) + ((x) >> 7) + ((x) >> 8) - ((x) >> 14))
#define FR_Q2RAD(x) ((x) + ((x) >> 1) + ((x) >> 4) + ((x) >> 7) + ((x) >> 11))
#define FR_DEG2Q(x) (((x) >> 6) - ((x) >> 8) - ((x) >> 11) - ((x) >> 13))
#define FR_Q2DEG(x) (((x) << 6) + ((x) << 4) + ((x) << 3) + ((x) << 1))

/*===============================================
 * BAM (Binary Angular Measure) — internal angle representation
 *
 * One full circle = 2^16 BAM units. So:
 *   0       = 0 deg     = 0 rad
 *   16384   = 90 deg    = pi/2 rad   (FR_BAM_QUADRANT)
 *   32768   = 180 deg   = pi rad
 *   49152   = 270 deg   = 3pi/2 rad
 *   65536   wraps to 0  (because BAM is u16)
 *
 * BAM is the natural representation for fixed-point trig because:
 *   - The top 2 bits select the quadrant (no `% 360` modulo needed).
 *   - The next 7 bits index the 128-entry quadrant table directly.
 *   - The bottom 7 bits give linear-interpolation precision.
 */
#define FR_BAM_BITS         (16)
#define FR_BAM_FULL         (1L << FR_BAM_BITS)         /* 65536 */
#define FR_BAM_QUADRANT     (FR_BAM_FULL >> 2)          /* 16384 */
#define FR_BAM_HALF         (FR_BAM_FULL >> 1)          /* 32768 */

/*===============================================
 * Radian-native and BAM-native trig (recommended)
 *
 * All sin/cos functions return s32 at radix 16 (s15.16).
 * 1.0 is represented exactly as FR_TRIG_ONE (65536).
 * Poles (0, 90, 180, 270 deg) produce exact ±FR_TRIG_ONE or 0.
 *
 *   fr_cos_bam(bam)         — cos of a BAM angle,        s15.16 result
 *   fr_sin_bam(bam)         — sin of a BAM angle,        s15.16 result
 *   fr_cos(rad, radix)      — cos of radians at radix,   s15.16 result
 *   fr_sin(rad, radix)      — sin of radians at radix,   s15.16 result
 *   fr_tan(rad, radix)      — tan of radians at radix,   s15.16 result
 *   fr_cos_deg(deg)         — cos of integer degrees,    s15.16 result
 *   fr_sin_deg(deg)         — sin of integer degrees,    s15.16 result
 *
 * All go through the same 129-entry quadrant table with linear interpolation.
 * Worst-case error: ~2 LSB in s15.16 (~3e-5 absolute), except at the four
 * cardinal angles where the result is exact.
 */
  s32 fr_cos_bam(u16 bam);
  s32 fr_sin_bam(u16 bam);
  s32 fr_tan_bam(u16 bam);
  s32 fr_cos(s32 rad, u16 radix);
  s32 fr_sin(s32 rad, u16 radix);
  s32 fr_tan(s32 rad, u16 radix);

/* Integer degrees -> BAM using division (exact at all multiples of 45 deg). */
#define FR_DEG2BAM_I(deg) ((u16)((((s32)(deg) << 16) + ((deg) >= 0 ? 180 : -180)) / 360))

#define fr_cos_deg(deg)  fr_cos_bam(FR_DEG2BAM_I(deg))
#define fr_sin_deg(deg)  fr_sin_bam(FR_DEG2BAM_I(deg))

/*===============================================
 * Integer-degree trig API (thin wrappers over the BAM-native path)
 *
 *   FR_CosI(deg)            — cos of integer degrees,       s15.16 result
 *   FR_SinI(deg)            — sin of integer degrees,       s15.16 result
 *   FR_TanI(deg)            — tan of integer degrees,       s15.16 result
 *   FR_Cos(deg, radix)      — cos of fixed-radix degrees,   s15.16 result
 *   FR_Sin(deg, radix)      — sin of fixed-radix degrees,   s15.16 result
 *   FR_Tan(deg, radix)      — tan of fixed-radix degrees,   s15.16 result
 */
#define FR_CosI(deg)  fr_cos_bam(FR_DEG2BAM_I(deg))
#define FR_SinI(deg)  fr_sin_bam(FR_DEG2BAM_I(deg))

  s32 FR_Cos(s32 deg, u16 radix);
  s32 FR_Sin(s32 deg, u16 radix);
  s32 FR_TanI(s32 deg);
  s32 FR_Tan(s32 deg, u16 radix);

  /* Inverse trig — output in radians at caller-specified radix (s32).
   * FR_atan2 returns radians at radix 16 (s15.16).
   * Range: acos [0, pi], asin [-pi/2, pi/2],
   *        atan [-pi/2, pi/2], atan2 [-pi, pi].
   */
  s32 FR_acos(s32 input, u16 radix, u16 out_radix);
  s32 FR_asin(s32 input, u16 radix, u16 out_radix);
  s32 FR_atan(s32 input, u16 radix, u16 out_radix);
  s32 FR_atan2(s32 y, s32 x, u16 out_radix);

/* Logarithms */
#define FR_LOG2MIN (-(32767 << 16)) /* returned instead of "negative infinity" */

  s32 FR_log2(s32 input, u16 radix, u16 output_radix);
  s32 FR_ln(s32 input, u16 radix, u16 output_radix);
  s32 FR_log10(s32 input, u16 radix, u16 output_radix);

  /* Power */
  s32 FR_pow2(s32 input, u16 radix);
#define FR_EXP(input, radix)   (FR_pow2(FR_MULK28((input), FR_kLOG2E_28), (radix)))
#define FR_POW10(input, radix) (FR_pow2(FR_MULK28((input), FR_kLOG2_10_28), (radix)))

/* Shift-only (multiply-free) base-conversion variants.
 * Lower accuracy (~5-10 LSB at Q16.16) but no multiply instruction.
 * Use these on targets where 32x32->64 multiply is expensive.
 */
#define FR_EXP_FAST(input, radix)   (FR_pow2(FR_SLOG2E(input), radix))
#define FR_POW10_FAST(input, radix) (FR_pow2(FR_SLOG2_10(input), radix))

/*===============================================
 * Formatted output and string parsing
 *
 * Define FR_NO_PRINT before including this header to exclude all
 * print/format functions from compilation. This saves ~1.7 KB of ROM
 * on targets that don't need human-readable output (e.g. headless
 * sensor nodes, DSP-only firmware).
 *
 *   #define FR_NO_PRINT
 *   #include "FR_math.h"
 */
#ifndef FR_NO_PRINT

  /* printing family of functions */
  int FR_printNumF(int (*f)(char), s32 n, int radix, int pad, int prec); /* print fixed radix num as floating point e.g.  -12.34" */
  int FR_printNumD(int (*f)(char), int n, int pad);                      /* print decimal number with optional padding e.g. " 12" */
  int FR_printNumH(int (*f)(char), int n, int showPrefix);               /* print num as a hexidecimal e.g. "0x12ab"              */

  /* string-to-fixed-point parser (inverse of FR_printNumF) */
  s32 FR_numstr(const char *s, u16 radix);

#endif /* FR_NO_PRINT */

/*===============================================
 * Square root and hypot
 *
 * Both take fixed-radix inputs and return a result at the same radix.
 * Algorithm: digit-by-digit isqrt on a 64-bit accumulator (no division,
 * at most 32 iterations). Rounds to nearest.
 *
 * Domain error sentinel: input < 0 (sqrt) returns FR_DOMAIN_ERROR. Caller
 * can check `result == FR_DOMAIN_ERROR` to detect domain errors.
 */
  s32 FR_sqrt(s32 input, u16 radix);
  s32 FR_hypot(s32 x, s32 y, u16 radix);

  /* Fast approximate magnitude — shift-only, no multiply, no 64-bit.
   * Based on piecewise-linear approximation of sqrt(x*x + y*y).
   * See US Patent 6,567,777 B1 (Chatterjee, expired).
   *
   *   FR_hypot_fast8(x, y)  8-segment, ~0.10% peak error
   *
   * Inputs are raw signed integers (or fixed-point at any radix — the
   * result is at the same radix as the inputs, just like FR_hypot).
   * No radix parameter needed because the algorithm is scale-invariant.
   */
  s32 FR_hypot_fast8(s32 x, s32 y);

/*===============================================
 * Wave generators and ADSR envelope
 *
 * Define FR_NO_WAVES before including this header to exclude all
 * waveform generators (square, pulse, triangle, saw, noise) and the
 * ADSR envelope from compilation. This saves ~400 B of ROM on targets
 * that only need math/trig and don't do audio synthesis.
 *
 *   #define FR_NO_WAVES
 *   #include "FR_math.h"
 */
#ifndef FR_NO_WAVES

/*===============================================
 * Wave generators — synth-style fixed-shape waveforms.
 *
 * All take a u16 BAM phase in [0, 65535] (a full cycle) and return s16
 * in s0.15 in [-32767, +32767]. Use FR_HZ2BAM_INC below to compute a
 * phase increment for a target frequency.
 *
 *   fr_wave_sqr(phase)              50% square
 *   fr_wave_pwm(phase, duty)        variable-duty pulse
 *   fr_wave_tri(phase)              symmetric triangle
 *   fr_wave_saw(phase)              rising sawtooth
 *   fr_wave_tri_morph(phase, brk)   variable-symmetry triangle (morphs to saw)
 *   fr_wave_noise(state*)           LFSR pseudorandom noise
 *
 * fr_wave_tri_morph returns [0, 32767] (unipolar) — caller can re-bias
 * if a bipolar form is desired. The other waves are bipolar [-32767, +32767].
 */
  s16 fr_wave_sqr(u16 phase);
  s16 fr_wave_pwm(u16 phase, u16 duty);
  s16 fr_wave_tri(u16 phase);
  s16 fr_wave_saw(u16 phase);
  s16 fr_wave_tri_morph(u16 phase, u16 break_point);
  s16 fr_wave_noise(u32 *state);

/* FR_HZ2BAM_INC(hz, sample_rate)
 * Compute the per-sample BAM phase increment for a target frequency in Hz
 * given a sample rate in Hz. Result is a u16 to add to the running phase
 * each sample (the running phase wraps mod 2^16 naturally because it's u16).
 *
 *   u16 phase = 0;
 *   u16 inc   = FR_HZ2BAM_INC(440, 48000);
 *   for (...) { sample = fr_sin_bam(phase); phase += inc; }
 *
 * Range: hz must be < sample_rate / 2 (Nyquist) for a meaningful tone;
 * higher hz aliases. The macro does not enforce this.
 *
 * Side-effect note: hz and sample_rate are evaluated once each.
 */
#define FR_HZ2BAM_INC(hz, sample_rate)  ((u16)(((u32)(hz) * 65536UL) / (u32)(sample_rate)))

/*===============================================
 * ADSR envelope generator
 *
 * Linear-segment Attack-Decay-Sustain-Release envelope. Caller-allocated
 * struct, no malloc, no global state. Internal level is held in s1.30 so
 * very long envelopes (e.g. 48000-sample attack at 48 kHz) still get a
 * non-zero per-sample increment. Output is s0.15 in [0, 32767].
 *
 * Lifecycle:
 *   fr_adsr_t env;
 *   fr_adsr_init(&env, atk_samples, dec_samples, sustain_s015, rel_samples);
 *   fr_adsr_trigger(&env);                 // note-on
 *   for (...) sample = fr_adsr_step(&env); // per-audio-sample
 *   fr_adsr_release(&env);                 // note-off
 *   for (...) sample = fr_adsr_step(&env); // until env.state == FR_ADSR_IDLE
 */
#define FR_ADSR_IDLE     (0)
#define FR_ADSR_ATTACK   (1)
#define FR_ADSR_DECAY    (2)
#define FR_ADSR_SUSTAIN  (3)
#define FR_ADSR_RELEASE  (4)

typedef struct fr_adsr_s {
    u8  state;        /* FR_ADSR_* */
    s32 level;        /* current envelope value, s1.30 */
    s32 sustain;      /* sustain target, s1.30 */
    s32 attack_inc;   /* per-sample increment during attack */
    s32 decay_dec;    /* per-sample decrement during decay */
    s32 release_dec;  /* per-sample decrement during release */
} fr_adsr_t;

  void fr_adsr_init(fr_adsr_t *env,
                    u32 attack_samples,
                    u32 decay_samples,
                    s16 sustain_level_s015,
                    u32 release_samples);
  void fr_adsr_trigger(fr_adsr_t *env);
  void fr_adsr_release(fr_adsr_t *env);
  s16  fr_adsr_step(fr_adsr_t *env);

#endif /* FR_NO_WAVES */

#ifdef __cplusplus

} // extern "C"
#endif

#endif /* __FR_Math_h__ */
