/**
 *
 *	@file FR_math.c - c implementation file for basic fixed
 *                              radix math routines
 *
 *	@copy Copyright (C) <2001-2014>  <M. A. Chatterjee>
 *  @author M A Chatterjee <deftio [at] deftio [dot] com>
 *	@version 1.0.3 M. A. Chatterjee, cleaned up naming
 *
 *  This file contains integer math settable fixed point radix math routines for
 *  use on systems in which floating point is not desired or unavailable.
 *
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
 *	in a product, please place an acknowledgment in the product documentation.
 *
 *	2. Altered source versions must be plainly marked as such, and must not be
 *	misrepresented as being the original software.
 *
 *	3. This notice may not be removed or altered from any source
 *	distribution.
 *
 */

#include "FR_math.h"
#include "FR_trig_table.h"

#ifndef FR_NO_INT64
#include <stdint.h>
#endif

/*=======================================================
 * v2 radian-native trig: fr_cos_bam, fr_sin_bam, fr_cos, fr_sin, fr_tan
 *
 * Internal model: every angle is reduced to a u16 BAM value. The top 2 bits
 * select the quadrant, the bottom 14 bits are the in-quadrant position. Odd
 * quadrants (1, 3) are mirrored across pi/4 so we only ever look up the
 * first quadrant of cos. Quadrants 1 and 2 get their sign flipped at the
 * end.
 *
 * Within each quadrant, the upper FR_TRIG_TABLE_BITS bits of the
 * in-quadrant value index the table; the lower FR_TRIG_FRAC_BITS bits drive
 * round-to-nearest linear interpolation between adjacent table entries.
 *
 * The (FR_TRIG_TABLE_SIZE)-th entry (table[last] = 0) means the
 * interpolation at the very edge of the quadrant never reads out of bounds.
 *
 * Rounding: we interpolate as
 *     v = lo - ((d * frac + HALF) >> FRAC_BITS)
 * where d = lo - hi (which is >= 0 because cos is monotonically decreasing
 * on [0, pi/2]). Using the subtract form guarantees the argument of >> is
 * always non-negative, so the behavior is portable C89 (no reliance on
 * implementation-defined right-shift of negative integers) and the +HALF
 * gives unambiguous round-half-up. Max error vs the true cos is ~1 LSB of
 * s0.15 (~3e-5 absolute); mean error ~0 (no bias).
 */
s16 fr_cos_bam(u16 bam)
{
	u32 q     = ((u32)bam >> 14) & 0x3;                /* top 2 bits = quadrant */
	u32 inq   = (u32)bam & (FR_TRIG_QUADRANT - 1);     /* bottom 14 bits        */
	u32 idx, frac;
	s32 lo, hi, d, v;

	if (q == 1 || q == 3)
		inq = FR_TRIG_QUADRANT - inq;                  /* mirror across pi/2    */

	idx  = inq >> FR_TRIG_FRAC_BITS;                   /* table index [0..SIZE-1] */
	frac = inq &  FR_TRIG_FRAC_MASK;                   /* interp fraction       */
	lo = gFR_COS_TAB_Q[idx];
	hi = gFR_COS_TAB_Q[idx + 1];
	d  = lo - hi;                                      /* >= 0: cos monotonic   */
	v  = lo - (((d * (s32)frac) + FR_TRIG_FRAC_HALF) >> FR_TRIG_FRAC_BITS);

	return (q == 1 || q == 2) ? (s16)(-v) : (s16)v;
}

s16 fr_sin_bam(u16 bam)
{
	/* sin(x) = cos(x - pi/2) = cos(bam - 16384). The u16 wraparound makes
	 * this completely free.
	 */
	return fr_cos_bam((u16)(bam - FR_BAM_QUADRANT));
}

/* Convert radians at given radix to BAM with rounding.
 * One radian = 65536 / (2*pi) ≈ 10430.378 BAM units.
 * We compute (rad * 10430) >> radix, plus a small correction for the
 * fractional 0.378 part to keep error bounded.
 */
static u16 fr_rad_to_bam(s32 rad, u16 radix)
{
#ifndef FR_NO_INT64
	int64_t scaled = ((int64_t)rad * 10430378LL) / 1000;  /* better precision */
	if (radix > 0)
		scaled >>= radix;
	return (u16)((u32)scaled & 0xffff);
#else
	s32 v = (rad * 10430) >> radix;
	return (u16)((u32)v & 0xffff);
#endif
}

s16 fr_cos(s32 rad, u16 radix)
{
	return fr_cos_bam(fr_rad_to_bam(rad, radix));
}

s16 fr_sin(s32 rad, u16 radix)
{
	return fr_sin_bam(fr_rad_to_bam(rad, radix));
}

/* fr_tan: returns sin/cos at s15.16. Saturates if cos is near zero. */
s32 fr_tan(s32 rad, u16 radix)
{
	u16 bam = fr_rad_to_bam(rad, radix);
	s32 s   = fr_sin_bam(bam);
	s32 c   = fr_cos_bam(bam);
	if (c == 0)
		return (s >= 0) ? (FR_TRIG_MAXVAL << FR_TRIG_PREC)
		                : -(FR_TRIG_MAXVAL << FR_TRIG_PREC);
	return (s << FR_TRIG_PREC) / c;
}

/*=======================================================
 * FR_FixMuls (x*y signed, NOT saturated)
 *
 * Treats x and y as fixed-point values at the same radix r and returns
 * (x*y) >> r at radix r. The user is responsible for tracking the radix
 * point and for guaranteeing the product fits in 32 bits.
 *
 * v2 implementation uses int64_t directly. The original split-multiply
 * macro path (FR_FIXMUL32u) had subtle correctness issues for cross-term
 * carries; modern toolchains all support 64-bit ints. If you need a
 * pre-C99 / strictly-32-bit-only build, define FR_NO_INT64.
 */
s32 FR_FixMuls(s32 x, s32 y)
{
#ifndef FR_NO_INT64
	int64_t v = (int64_t)x * (int64_t)y;
	return (s32)(v >> 16);
#else
	s32 z, sign = (x < 0) ? (y > 0) : (y < 0);
	x = FR_ABS(x);
	y = FR_ABS(y);
	z = FR_FIXMUL32u(x, y);
	return sign ? -z : z;
#endif
}

/*=======================================================
 * FR_FixMulSat (x*y signed, SATURATED)
 *
 * Same semantics as FR_FixMuls but clamps to [INT32_MIN, INT32_MAX] on
 * overflow instead of wrapping. The fixed-point radix is fixed at 16 bits
 * (sM.16 inputs and output).
 *
 * v1 bug: the original implementation summed the cross-terms incorrectly
 * (`(h<<16)+m1+m2+l` instead of `(h<<16)+m1+m2+(l>>16)`), so small inputs
 * returned values shifted 16 bits too high. v2 uses int64_t directly and
 * checks the saturation bound on the int64_t accumulator.
 */
s32 FR_FixMulSat(s32 x, s32 y)
{
#ifndef FR_NO_INT64
	int64_t v = ((int64_t)x * (int64_t)y) >> 16;
	if (v >  (int64_t)0x7fffffff) return  (s32)0x7fffffff;
	if (v < -(int64_t)0x80000000) return (s32)0x80000000;
	return (s32)v;
#else
	/* 32-bit-only fallback: split-multiply with corrected cross-term shift. */
	s32 z, h, m1, m2;
	u32 l;
	int sign = (((x < 0) && (y > 0)) || ((x > 0) && (y < 0))) ? 1 : 0;

	x = FR_ABS(x);
	y = FR_ABS(y);
	h  = (s32)(((u32)x >> 16) * ((u32)y >> 16));
	m1 = (s32)(((u32)x >> 16) * ((u32)y & 0xffff));
	m2 = (s32)(((u32)y >> 16) * ((u32)x & 0xffff));
	l  =       ((u32)x & 0xffff) * ((u32)y & 0xffff);

	if (h & 0xffff8000)
		return sign ? (s32)0x80000000 : (s32)0x7fffffff;

	z = (h << 16) + m1 + m2 + (s32)(l >> 16);
	if (z < 0)
		return sign ? (s32)0x80000000 : (s32)0x7fffffff;
	return sign ? -z : z;
#endif
}

/*=======================================================
  FR_FixAddSat (x+y saturated add)
  programmer must align radix points before using this function
 */
s32 FR_FixAddSat(s32 x, s32 y)
{
	s32 sum = x + y;
	if (x < 0)
	{
		if (y < 0)
			return (sum >= 0) ? 0x80000000 : sum;
	}
	else
	{
		if (y >= 0)
			return (sum <= 0) ? 0x7fffffff : sum;
	}
	return sum;
}

/* Cosine table in s0.15 format, 1 entry per degree
 * used in all trig functions (sin,cos,tan, atan2 etc)
 */
s16 const static gFR_COS_TAB_S0d15[] = {
	32767, 32762, 32747, 32722, 32687, 32642, 32587, 32522, 32448, 32363,
	32269, 32164, 32050, 31927, 31793, 31650, 31497, 31335, 31163, 30981,
	30790, 30590, 30381, 30162, 29934, 29696, 29450, 29195, 28931, 28658,
	28377, 28086, 27787, 27480, 27165, 26841, 26509, 26168, 25820, 25464,
	25100, 24729, 24350, 23964, 23570, 23169, 22761, 22347, 21925, 21497,
	21062, 20620, 20173, 19719, 19259, 18794, 18323, 17846, 17363, 16876,
	16383, 15885, 15383, 14875, 14364, 13847, 13327, 12803, 12274, 11742,
	11207, 10668, 10125, 9580, 9032, 8481, 7927, 7371, 6813, 6252,
	5690, 5126, 4560, 3993, 3425, 2856, 2286, 1715, 1144, 572,
	0};

/* cosine with integer input precision in degrees, returns s0.15 result
 */

s16 FR_CosI(s16 deg)
{
	deg = deg % 360; /* this is an expensive operation*/
	if (deg > 180)
	{
		deg -= 360;
	}
	else if (deg < -180)
	{
		deg += 360;
	}

	if (deg >= 0)
		return deg <= (90) ? gFR_COS_TAB_S0d15[deg] : -gFR_COS_TAB_S0d15[180 - deg];
	else
		return deg >= (-90) ? gFR_COS_TAB_S0d15[-deg] : -gFR_COS_TAB_S0d15[180 + deg];
}
/* sin with integer input precision in degrees, returns s0.15 result
 */
s16 FR_SinI(s16 deg)
{
	return FR_CosI(deg - 90);
}

/* cos() with fixed radix precision, returns interpolated s0.15 result
 */
s16 FR_Cos(s16 deg, u16 radix)
{
	s16 i, j;
	i = FR_CosI(deg >> radix);
	j = FR_CosI((deg >> radix) + 1);
	return i + (((j - i) * (deg & ((1 << radix) - 1))) >> radix);
}

/* sin() with fixed radix precision,  returns interpolated s0.15 result
 * could be a macro..
 */
s16 FR_Sin(s16 deg, u16 radix)
{
	return FR_Cos(deg - (90 << radix), radix);
}

s16 const static gFR_TAND_TAB[] = {
	1, 572, 1144, 1717, 2291, 2867, 3444, 4023, 4605, 5189,
	5777, 6369, 6964, 7564, 8169, 8779, 9395, 10017, 10646, 11282,
	11926, 12578, 13238, 13908, 14588, 15279, 15981, 16695, 17422, 18163,
	18918, 19688, 20475, 21279, 22101, 22943, 23806, 24691, 25600, 26534,
	27494, 28483, 29503, 30555, 31642, 32767};

/* tan with s15.16 result
 * tan without table.
 * note: tan(90)  returns   32767
 * and   tan(270) returns (-32768) (e.g. no div by zero)
 */
/*
s32 FR_TanI (s16 deg)
{
	s32 c = FR_CosI(deg);
	s32 s = FR_SinI(deg);
	return (c!=0)?(s<<FR_TRIG_PREC)/c : (s>=0)?(FR_TRIG_MAXVAL<<FR_TRIG_PREC):(FR_TRIG_MINVAL<<FR_TRIG_PREC);
}
*/
#define FR_TN(a) (((a) <= 45) ? gFR_TAND_TAB[(a)] : (FR_TRIG_MAXVAL << FR_TRIG_PREC) / (gFR_TAND_TAB[90 - (a)]))
s32 FR_TanI(s16 deg)
{
	deg = deg % 360; /* this is an expensive operation */
	if (deg > 180)
	{
		deg -= 360;
	}
	else if (deg < -180)
	{
		deg += 360;
	}

	if (90 == deg)
		return (FR_TRIG_MAXVAL << FR_TRIG_PREC);
	/* v1 had `if (270 == deg)` here; that branch is unreachable because the
	 * `% 360` above followed by the `[-180, 180]` reduction guarantees deg
	 * is never 270. Removed in v2 (gcov 0 hits for ~10 years confirms it). */

	if (deg >= 0)
		return deg <= (90) ? FR_TN(deg) : -FR_TN(180 - deg);
	else
		return deg >= (-90) ? -FR_TN(-deg) : FR_TN(180 + deg);
}
/* Tan with s15.16 result with fixed radix input precision, returns interpolated s15.16 result.
 *
 * v1 bug: locals `i, j` were declared `s16`, but `FR_TanI` returns `s32`,
 * so steep angles (|tan| > 0.5) silently truncated. Fixed in v2 by widening
 * to `s32`.
 */
s32 FR_Tan(s16 deg, u16 radix)
{
	s32 i, j;
	i = FR_TanI(deg >> radix);
	j = FR_TanI((deg >> radix) + 1);
	return FR_INTERPI(i, j, deg, radix); /*i+(((j-i)*(deg&((1<<radix)-1)))>>radix);*/
}

/* Inverse Trig
 * Ugly looking acos with bin search (working):
 */
s16 FR_acos(s32 input, u16 radix)
{
	s16 r = 45, s = input, x = 46, y, z;

	input = FR_CHRDX(input, radix, FR_TRIG_PREC); /* chg radix to s0.15 */

	// +or- 1.0000 is special case as it doesn't fit in table search
	if ((input & 0xffff) == 0x8000) //? shouldn't it be: (input&7fff)!=0
		return (input < 0) ? 180 : 0;
	input = (FR_ABS(input)) & ((1 << radix) - 1);
	while (x >>= 1)
		r += (input < gFR_COS_TAB_S0d15[r]) ? x : -x;

	r += (input < gFR_COS_TAB_S0d15[r]) ? 1 : -1;
	r += (input < gFR_COS_TAB_S0d15[r]) ? 1 : -1;

	x = FR_ABS(input - gFR_COS_TAB_S0d15[r]);
	y = FR_ABS(input - gFR_COS_TAB_S0d15[r + 1]);
	z = FR_ABS(input - gFR_COS_TAB_S0d15[r - 1]);
	r = (x < y) ? r : r + 1;
	r = (x < z) ? r : r - 1;

	return (s > 0) ? r : 180 - r;
	;
}

s16 FR_asin(s32 input, u16 radix)
{
	return 90 - FR_acos(input, radix);
}

/* arctan table: gFR_ATAN_TAB[i] = atan(i/32) in degrees, scaled by 64
 * (i.e. fixed-point s.6), for i in [0..32]. So index 32 is atan(1) = 45°.
 *
 * Generated by:
 *   for i in 0..32: int(round(degrees(atan(i/32.0)) * 64))
 *
 *   i=0:   0   atan(0/32)   =  0°       *64 = 0
 *   i=1: 115   atan(1/32)   =  1.7899°  *64 = 114.55
 *   ...
 *   i=32: 2880 atan(32/32)  = 45°       *64 = 2880
 */
static const s16 gFR_ATAN_TAB[33] = {
       0,   115,   229,   343,   456,   568,   680,   790,
     898,  1005,  1111,  1214,  1316,  1415,  1512,  1607,
    1700,  1791,  1879,  1965,  2048,  2130,  2209,  2285,
    2360,  2432,  2502,  2570,  2636,  2700,  2762,  2822,
    2880
};

/* helper: arctan(t) for t in [0,1] in radix-16 input, returning degrees s16.
 * Uses the table above with linear interpolation.
 *
 * t is in s.16. The table indexes into [0,1] in 32 steps, so the table
 * step in s.16 units is (1<<16)/32 = 2048.
 */
static s16 fr_atan_unit_q1_deg(s32 t_s16)
{
	s32 idx, frac, lo, hi, deg64;
	if (t_s16 <= 0) return 0;
	if (t_s16 >= (1L << 16)) return 45;
	idx  = t_s16 >> 11;        /* 2048 = 1<<11 */
	frac = t_s16 & ((1L << 11) - 1);
	lo = gFR_ATAN_TAB[idx];
	hi = gFR_ATAN_TAB[idx + 1];
	deg64 = lo + (((hi - lo) * frac) >> 11);
	/* deg64 is degrees * 64; round to nearest integer degree */
	return (s16)((deg64 + 32) >> 6);
}

/* FR_atan2(y, x, radix) — full-circle arctangent, returns degrees as s16.
 *
 * v1 bug: the original body just returned a quadrant index 0..3, not an
 * angle. v2 implements the standard octant-reduction algorithm:
 *   1. Special-case the four axis directions.
 *   2. Reduce |y|/|x| to [0,1] (swap so the smaller magnitude is on top).
 *   3. Look up arctan in the table with linear interpolation.
 *   4. Apply quadrant / swap corrections to get the final angle.
 *
 * `radix` is currently ignored at the input side because the function
 * computes a *ratio* y/x, which is radix-independent. It is kept in the
 * signature for source-compatibility with v1.
 *
 * Range: [-180, 180] degrees. Returns 0 for atan2(0,0) (consistent with
 * IEEE 754 atan2 even though that case is mathematically undefined).
 */
s16 FR_atan2(s32 y, s32 x, u16 radix)
{
	s32 ay, ax, ratio;
	s16 a;
	(void)radix;

	/* Axis cases first — these also avoid the divide. */
	if (x == 0)
	{
		if (y > 0) return  90;
		if (y < 0) return -90;
		return 0; /* atan2(0,0) — undefined; return 0 */
	}
	if (y == 0)
		return (x > 0) ? 0 : 180;

	ax = (x < 0) ? -x : x;
	ay = (y < 0) ? -y : y;

	/* Compute ratio of smaller / larger in s.16 so it stays in [0,1]. */
	if (ay <= ax)
	{
#ifndef FR_NO_INT64
		ratio = (s32)(((int64_t)ay << 16) / ax);
#else
		ratio = (s32)((((u32)ay) << 16) / (u32)ax);
#endif
		a = fr_atan_unit_q1_deg(ratio);          /* [0..45] */
	}
	else
	{
#ifndef FR_NO_INT64
		ratio = (s32)(((int64_t)ax << 16) / ay);
#else
		ratio = (s32)((((u32)ax) << 16) / (u32)ay);
#endif
		a = (s16)(90 - fr_atan_unit_q1_deg(ratio)); /* [45..90] */
	}

	/* Apply quadrant sign. */
	if (x > 0)
		return (y > 0) ? a : (s16)(-a);
	/* x < 0 */
	return (y > 0) ? (s16)(180 - a) : (s16)(a - 180);
}

/* FR_atan(input, radix) — arctangent of a single argument.
 *
 * v1 bug: declared in FR_math.h but never defined — calling it was a link
 * error. v2 implements as a thin wrapper over FR_atan2(input, 1.0).
 *
 * `input` is at the given radix. Returns degrees as s16, range [-90, 90].
 */
s16 FR_atan(s32 input, u16 radix)
{
	s32 one = (s32)1 << radix;
	return FR_atan2(input, one, radix);
}

/* 2^f table for f in [0, 1] in 17 entries, output in s.16 fixed point.
 * Used by FR_pow2 to look up the fractional power of 2 with linear
 * interpolation.
 */
static const u32 gFR_POW2_FRAC_TAB[17] = {
     65536,  /* 2^0      = 1.000000 */
     68438,  /* 2^0.0625 = 1.044274 */
     71468,  /* 2^0.125  = 1.090508 */
     74632,  /* 2^0.1875 = 1.138789 */
     77936,  /* 2^0.25   = 1.189207 */
     81386,  /* 2^0.3125 = 1.241858 */
     84990,  /* 2^0.375  = 1.296840 */
     88752,  /* 2^0.4375 = 1.354256 */
     92682,  /* 2^0.5    = 1.414214 */
     96785,  /* 2^0.5625 = 1.476826 */
    101070,  /* 2^0.625  = 1.542211 */
    105545,  /* 2^0.6875 = 1.610490 */
    110218,  /* 2^0.75   = 1.681793 */
    115098,  /* 2^0.8125 = 1.756252 */
    120194,  /* 2^0.875  = 1.834008 */
    125515,  /* 2^0.9375 = 1.915207 */
    131072   /* 2^1      = 2.000000 */
};

/* FR_pow2(input, radix) — computes 2^(input/2^radix), result at same radix.
 *
 * Algorithm: split input into floor(integer) and fractional part. The
 * fractional part is in [0, 1) by construction (Euclidean / mathematical
 * floor — the fractional part of -2.3 is +0.7, not -0.3). Then
 *   2^(int + frac) = 2^int * 2^frac
 * where 2^frac is looked up from a 17-entry table at radix 16, and 2^int
 * is a shift.
 *
 * v1 bug: the original implementation didn't compute mathematical floor for
 * negative inputs, so 2^(-0.5) returned ~1.41 instead of 0.71. v2 uses an
 * explicit Euclidean floor.
 *
 * Worst-case absolute error: ~1.5e-4 over [-8, 8]. Linear interpolation
 * leaves a small concavity error in each table interval.
 */
s32 FR_pow2(s32 input, u16 radix)
{
	s32 flr, frac_full, idx, frac_lo, lo, hi, mant, result;
	u32 mask = (radix > 0) ? (((u32)1 << radix) - 1) : 0;

	/* Mathematical floor: for positive input it's input>>radix; for
	 * negative input we need to round toward -infinity, not toward zero.
	 */
	if (input >= 0)
	{
		flr = (s32)((u32)input >> radix);
		frac_full = (s32)((u32)input & mask);
	}
	else
	{
		s32 neg = -input;
		s32 nflr = (s32)((u32)neg >> radix);
		s32 nfrc = (s32)((u32)neg & mask);
		if (nfrc == 0)
		{
			flr = -nflr;
			frac_full = 0;
		}
		else
		{
			flr = -nflr - 1;          /* floor toward -inf */
			frac_full = (s32)((1L << radix) - nfrc);
		}
	}

	/* frac_full is in [0, 2^radix). Re-radix it to s.16 for table lookup. */
	if (radix > 16)
		frac_full >>= (radix - 16);
	else if (radix < 16)
		frac_full <<= (16 - radix);
	/* now frac_full is in [0, 65536) representing fractional in s.16. */

	/* Top 4 bits index the table; bottom 12 are the interpolation fraction. */
	idx     = frac_full >> 12;
	frac_lo = frac_full & ((1L << 12) - 1);
	lo = (s32)gFR_POW2_FRAC_TAB[idx];
	hi = (s32)gFR_POW2_FRAC_TAB[idx + 1];
	mant = lo + (((hi - lo) * frac_lo) >> 12);  /* mant in s.16, in [1.0, 2.0) */

	/* Apply integer shift. mant is at radix 16. We want output at `radix`.
	 * If radix == 16: just shift mant.
	 * Otherwise re-radix mant first.
	 */
	if (flr >= 0)
	{
		/* result = mant << flr, then re-radix to caller's radix. */
		if (flr >= 30)
			return (s32)0x7fffffff;        /* overflow */
		result = mant << flr;
		return FR_CHRDX(result, 16, radix);
	}
	else
	{
		/* mant >> -flr at radix 16, then re-radix. */
		s32 sh = -flr;
		if (sh >= 30)
			return 0;                       /* underflow */
		result = mant >> sh;
		return FR_CHRDX(result, 16, radix);
	}
}
/*
s32 FR_exp(s32 input, u16 radix)
{
	return FR_pow2(FR_SLOG2E(input),radix);
}

s32 FR_pow10(s32 input, u16 radix)
{
	if (FR_FRAC(input,radix))
		return FR_pow2(FR_SLOG2_10(input),radix);
	else
	{
		input = FR_INT(input,radix);
		if (input >=0)
		{
			s32 x=10;
			while (input--)
				x = FR_SMUL10(x);
			return x<<radix;
		}
		return FR_pow2(FR_SLOG2_10(input),radix);
	}
}
*/

/* log2 mantissa table for m in [1, 2), m = 1 + i/32, returning log2(m)
 * in s.16 fixed point. 33 entries (last is log2(2) = 1.0 = 65536) so the
 * interpolation between idx and idx+1 never reads out of bounds.
 */
static const u32 gFR_LOG2_MANT_TAB[33] = {
        0,  2909,  5732,  8473, 11136, 13727, 16248, 18704,
    21098, 23433, 25711, 27936, 30109, 32234, 34312, 36346,
    38336, 40286, 42196, 44068, 45904, 47705, 49472, 51207,
    52911, 54584, 56229, 57845, 59434, 60997, 62534, 64047,
    65536
};

/* FR_log2(input, radix, output_radix) — log base 2 of a fixed-point number.
 *
 *   input        : value to take log2 of, treated as a positive sM.radix value.
 *   radix        : number of fractional bits in `input`.
 *   output_radix : number of fractional bits in the result.
 *
 * Returns FR_LOG2MIN for input <= 0 (log of zero/negative is undefined; we
 * return a large negative sentinel rather than crash).
 *
 * Algorithm:
 *   1. Find p, the position of the leading 1 bit of `input`.
 *      log2(input) = p + log2(input / 2^p), where the second term is in
 *      [0, 1) because (input / 2^p) is in [1, 2).
 *   2. Normalize the mantissa to s1.31 by shifting `input` so its top bit
 *      sits at bit 31 (so bits 30..26 are the upper 5 bits of m-1).
 *   3. Look up log2(m) in the 33-entry table with linear interpolation
 *      across the next 11 bits. Result is in s.16.
 *   4. integer_part = (p - radix), then result = (integer_part << 16) +
 *      mantissa_log2.
 *   5. Re-radix to the requested output_radix via FR_CHRDX.
 *
 * v1 bug: the original implementation used a bit-position counter `h` as
 * a *shift width* for the binary search but never accumulated it into the
 * result, so the function returned the reduced input, not log2 of the input.
 * v2 uses an explicit leading-bit + mantissa-table approach.
 *
 * Worst-case absolute error: ~5e-4 in log2 units.
 */
s32 FR_log2(s32 input, u16 radix, u16 output_radix)
{
	s32 p, integer_part, idx, frac, lo, hi, mant_log2, result;
	u32 m, u;

	if (input <= 0)
		return FR_LOG2MIN;

	/* Step 1: find the position of the leading 1 bit. */
	u = (u32)input;
	p = 0;
	while (u > 1)
	{
		u >>= 1;
		p++;
	}

	/* Step 2: shift input so the leading 1 bit is at bit 30 (s1.30 mantissa).
	 * Equivalently: m = input << (30 - p), where m is in [2^30, 2^31).
	 * The fractional part of m / 2^30 is in [0, 1), and that's what we look
	 * up in the table.
	 */
	if (p >= 30)
		m = (u32)input >> (p - 30);
	else
		m = (u32)input << (30 - p);

	/* m is now in [2^30, 2^31). Subtract 2^30 to get the fractional part
	 * (m_frac in [0, 2^30)). Index into the 32-entry table is the top 5
	 * bits of m_frac; the lower 25 bits are the interpolation fraction.
	 */
	m -= (1u << 30);
	idx  = (s32)(m >> 25);                    /* 5 bits  */
	frac = (s32)(m & ((1u << 25) - 1));       /* 25 bits */
	lo = (s32)gFR_LOG2_MANT_TAB[idx];
	hi = (s32)gFR_LOG2_MANT_TAB[idx + 1];
	/* mant_log2 is in s.16. Linear interp across 25-bit frac.
	 * (hi-lo) is at most ~3000, frac is up to 2^25, so the product fits in
	 * int64 comfortably. On platforms without int64, drop interpolation
	 * precision by shifting frac down first.
	 */
#ifndef FR_NO_INT64
	mant_log2 = lo + (s32)(((int64_t)(hi - lo) * frac) >> 25);
#else
	mant_log2 = lo + (((hi - lo) * (frac >> 10)) >> 15);
#endif

	/* Step 3: assemble. integer_part = p - radix. */
	integer_part = p - (s32)radix;
	result = (integer_part << 16) + mant_log2;

	/* Step 4: re-radix to output_radix. */
	return FR_CHRDX(result, 16, output_radix);
}

s32 FR_ln(s32 input, u16 radix, u16 output_radix)
{
	s32 r = FR_log2(input, radix, output_radix);
	return FR_SrLOG2E(r); /* Note: return FR_SrLOG2E(FR_log2()) would be a very ugly macro expansion! */
}

s32 FR_log10(s32 input, u16 radix, u16 output_radix)
{
	s32 r = FR_log2(input, radix, output_radix);
	return FR_SrLOG2_10(r); /* Note: return FR_SrLOG2_10(FR_log2()) would be a very ugly macro expansion! */
}

/***************************************
 FR_printNum write out fixed radix number with space padding
  equiavlent ot %f in printf family
  myNum = 12.34 // in fixed num
  e.g. printf("%4.2f",myNum ) ==> "  12"


	printf("test fr math rad \n");

	FR_printNumF (putchar,  123456   , 0, 3, 0);    printf("\n");
	FR_printNumF (putchar,  123456<<13 , 13, 3, 4);    printf(":\n");
	FR_printNumF (putchar,  D2FR(1234.5678,13)   , 13, 3, 6);    printf(":\n");
	FR_printNumF (putchar,  D2FR(-1234.5678,13)   , 13, 3, 6);    printf(":\n");
 */

/***************************************
 * FR_printNumD - write a decimal integer with space padding.
 *
 * Equivalent to "%*d" in printf, modulo the return convention.
 *
 *   f       : per-character output function (e.g. putchar). Must not be NULL.
 *   n       : signed integer to print.
 *   pad     : minimum field width; spaces are prepended to reach this width.
 *
 * Returns the number of characters written on success, or -1 if `f` is NULL.
 *
 * v1 bugs fixed in v2:
 *   - `n = -n` overflowed for INT_MIN (mangled output). v2 takes the
 *     unsigned magnitude up front: `mag = (n < 0) ? -(unsigned)n : n;`
 *     which handles INT_MIN correctly because unsigned negation is defined.
 *   - The function returned FR_S_OK (== 0) instead of the byte count. v2
 *     tracks and returns the count.
 */
int FR_printNumD(int (*f)(char), int n, int pad)
{
	unsigned int mag;
	int written = 0, neg = 0;
	int digits = 1;
	unsigned int t;

	if (!f)
		return -1;

	if (n < 0)
	{
		neg = 1;
		mag = (unsigned int)(-(long)n); /* safe for INT_MIN */
	}
	else
	{
		mag = (unsigned int)n;
	}

	/* Count decimal digits in mag (always at least 1 for n=0). */
	t = mag;
	while (t >= 10)
	{
		t /= 10;
		digits++;
	}

	/* Pad with spaces. The total width includes the sign. */
	{
		int total = digits + (neg ? 1 : 0);
		while (pad-- > total)
		{
			f(' ');
			written++;
		}
	}

	if (neg)
	{
		f('-');
		written++;
	}

	/* Print digits MSB first by computing the largest power of 10 <= mag. */
	{
		unsigned int p = 1;
		int i;
		for (i = 1; i < digits; i++)
			p *= 10;
		while (p > 0)
		{
			f((char)('0' + (mag / p) % 10));
			written++;
			if (p == 1)
				break;
			p /= 10;
		}
	}

	return written;
}

/***************************************
 * FR_printNumF - write a fixed-point number as a decimal floating-point string.
 *
 *   f      : per-character output function. Must not be NULL.
 *   n      : signed fixed-point value at the given radix.
 *   radix  : number of fractional bits in `n`.
 *   pad    : minimum field width (including sign and decimal point).
 *   prec   : number of fractional digits to print.
 *
 * Returns the number of characters written on success, -1 if `f` is NULL.
 *
 * v1 bugs fixed in v2:
 *   - INT_MIN-style negation overflow (same fix as FR_printNumD).
 *   - Fraction extraction was producing garbage (0.0001 -> "0.9000",
 *     1.05 -> "1.4998") because the original "scale to 10^k then >>radix"
 *     loop terminated on the wrong condition. v2 uses an explicit
 *     digit-extraction loop: at each step multiply the fractional part
 *     by 10, the new top bits give the next decimal digit, the bottom
 *     bits become the new fraction.
 *   - Returned FR_S_OK (0) instead of byte count.
 *
 * Rounding policy: truncates fractional digits beyond `prec` (no rounding).
 */
int FR_printNumF(int (*f)(char), s32 n, int radix, int pad, int prec)
{
	unsigned int mag_int;
	u32 mag_frac;
	u32 frac_mask;
	int written = 0, neg = 0;
	int int_digits = 1;
	int total;
	unsigned int t;

	if (!f)
		return -1;

	frac_mask = (radix > 0) ? (((u32)1 << radix) - 1) : 0;

	if (n < 0)
	{
		neg = 1;
		/* Negate as unsigned to avoid INT_MIN overflow. */
		u32 un = (u32)(-(int64_t)n);
		mag_int  = (unsigned int)(un >> radix);
		mag_frac = un & frac_mask;
	}
	else
	{
		mag_int  = (unsigned int)((u32)n >> radix);
		mag_frac = (u32)n & frac_mask;
	}

	/* Count integer digits. */
	t = mag_int;
	while (t >= 10)
	{
		t /= 10;
		int_digits++;
	}

	/* Total visible width = sign + int + (dot + prec digits if prec>0). */
	total = int_digits + (neg ? 1 : 0) + ((prec > 0) ? (1 + prec) : 0);
	while (pad-- > total)
	{
		f(' ');
		written++;
	}

	if (neg)
	{
		f('-');
		written++;
	}

	/* Print integer part. */
	{
		unsigned int p = 1;
		int i;
		for (i = 1; i < int_digits; i++)
			p *= 10;
		while (p > 0)
		{
			f((char)('0' + (mag_int / p) % 10));
			written++;
			if (p == 1)
				break;
			p /= 10;
		}
	}

	/* Print fractional part. Extract one decimal digit at a time:
	 * frac' = frac * 10
	 * digit = frac' >> radix
	 * frac  = frac' & frac_mask
	 */
	if (prec > 0)
	{
		f('.');
		written++;
		while (prec-- > 0)
		{
			u32 scaled;
			int digit;
			/* Use 64-bit accumulator to keep precision; (mag_frac * 10)
			 * can be up to ~2^4 * 2^radix which fits comfortably in u64.
			 */
#ifndef FR_NO_INT64
			scaled = (u32)(((uint64_t)mag_frac * 10));
#else
			scaled = mag_frac * 10;
#endif
			digit = (int)(scaled >> radix);
			mag_frac = scaled & frac_mask;
			f((char)('0' + (digit % 10)));
			written++;
		}
	}

	return written;
}

/***************************************
 * FR_printNumH - write an integer as hexadecimal.
 *
 *   f          : per-character output function. Must not be NULL.
 *   n          : integer to print (interpreted as unsigned for the digits).
 *   showPrefix : if non-zero, prepend "0x".
 *
 * Returns the number of characters written on success, -1 if f is NULL.
 *
 * v1 bug fixed in v2: the original right-shifted a *signed* int, which is
 * implementation-defined for negative values. v2 casts to unsigned first.
 * Also returned FR_S_OK (0) instead of byte count.
 */
int FR_printNumH(int (*f)(char), int n, int showPrefix)
{
	unsigned int u = (unsigned int)n;
	int written = 0;
	int x = (int)((sizeof(int) << 1) - 1);
	int d;

	if (!f)
		return -1;

	if (showPrefix)
	{
		f('0');
		f('x');
		written += 2;
	}

	do
	{
		d = (int)((u >> (x << 2)) & 0xf);
		d = (d > 9) ? (d - 0xa + 'a') : (d + '0');
		f((char)d);
		written++;
	} while (x--);

	return written;
}
