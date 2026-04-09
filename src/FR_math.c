/**
 *
 *	@file FR_math.c - c implementation file for basic fixed
 *                              radix math routines
 *
 *	@copy Copyright (C) <2001-2026>  <M. A. Chatterjee>
 *  @author M A Chatterjee <deftio [at] deftio [dot] com>
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

#include <stdint.h>

/*=======================================================
 * BAM-native trig: fr_cos_bam, fr_sin_bam, fr_cos, fr_sin, fr_tan
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
 * We use the more precise scaled constant 10430378 / 1000 to keep error
 * bounded across a wide range of radians.
 */
static u16 fr_rad_to_bam(s32 rad, u16 radix)
{
	int64_t scaled = ((int64_t)rad * 10430378LL) / 1000;
	if (radix > 0)
		scaled >>= radix;
	return (u16)((u32)scaled & 0xffff);
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
 * Integer-degree and fixed-radix-degree trig wrappers
 *
 * FR_CosI / FR_SinI are macros in the header (zero cost). The fixed-radix
 * variants here convert s.r degrees to BAM in one shot using a precomputed
 * reciprocal of 360 to avoid division on multiply-poor cores like 8051.
 *
 * Math: bam = deg * (65536 / 360) = deg * 182.0444...
 * In s.16 fixed point: 65536 / 360 = 0xB60B (rounded). So
 *   bam_u16 = (deg_s.r * 0xB60B) >> r
 * gives bam in u16 BAM units. The constant 0xB60B contains the divide by
 * 360 baked in; the shift `>> r` strips the input radix.
 */
static u16 fr_deg_radix_to_bam(s16 deg, u16 radix)
{
	/* (s32)deg * 0xB60B keeps everything in 32-bit math (8051-friendly).
	 * For radix 0, 0xB60B = 65536/360 ≈ 182.0444. The shift strips the
	 * input radix to land in u16 BAM space.
	 */
	s32 v = (s32)deg * 0xB60BL;
	return (u16)((u32)(v >> radix) & 0xffff);
}

s16 FR_Cos(s16 deg, u16 radix)
{
	return fr_cos_bam(fr_deg_radix_to_bam(deg, radix));
}

s16 FR_Sin(s16 deg, u16 radix)
{
	return fr_sin_bam(fr_deg_radix_to_bam(deg, radix));
}

s32 FR_TanI(s16 deg)
{
	u16 bam = FR_DEG2BAM(deg);
	s32 s   = fr_sin_bam(bam);
	s32 c   = fr_cos_bam(bam);
	if (c == 0)
		return (s >= 0) ? (FR_TRIG_MAXVAL << FR_TRIG_PREC)
		                : -(FR_TRIG_MAXVAL << FR_TRIG_PREC);
	return (s << FR_TRIG_PREC) / c;
}

s32 FR_Tan(s16 deg, u16 radix)
{
	u16 bam = fr_deg_radix_to_bam(deg, radix);
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
 */
s32 FR_FixMuls(s32 x, s32 y)
{
	int64_t v = (int64_t)x * (int64_t)y;
	return (s32)(v >> 16);
}

/*=======================================================
 * FR_FixMulSat (x*y signed, SATURATED)
 *
 * Same semantics as FR_FixMuls but clamps to [INT32_MIN, INT32_MAX] on
 * overflow instead of wrapping. The fixed-point radix is fixed at 16 bits
 * (sM.16 inputs and output).
 */
s32 FR_FixMulSat(s32 x, s32 y)
{
	int64_t v = ((int64_t)x * (int64_t)y) >> 16;
	if (v >  (int64_t)0x7fffffff) return  FR_OVERFLOW_POS;
	if (v < -(int64_t)0x80000000) return  FR_OVERFLOW_NEG;
	return (s32)v;
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
			return (sum >= 0) ? FR_OVERFLOW_NEG : sum;
	}
	else
	{
		if (y >= 0)
			return (sum <= 0) ? FR_OVERFLOW_POS : sum;
	}
	return sum;
}

/* Inverse Trig
 * acos with binary search of the BAM-native quadrant table.
 *
 * Algorithm: bring `input` into s0.15, then binary-search the first-quadrant
 * cos table for the table entry closest to |input|. Apply quadrant mirror
 * if input was negative.
 */
s16 FR_acos(s32 input, u16 radix)
{
	s32 v;
	s16 sign;
	s32 lo, hi, mid;
	s32 best_idx, best_err;
	s32 left, right;

	v = FR_CHRDX(input, radix, FR_TRIG_PREC); /* to s0.15 */

	/* Clamp range. */
	if (v >=  32767) return 0;
	if (v <= -32767) return 180;

	sign = (v < 0) ? 1 : 0;
	if (v < 0) v = -v;

	/* Binary search on the BAM quadrant table. The table is monotonically
	 * decreasing across [0, FR_TRIG_TABLE_SIZE]. We want the index `i`
	 * such that gFR_COS_TAB_Q[i] is closest to v.
	 */
	lo = 0;
	hi = FR_TRIG_TABLE_SIZE;
	while (lo < hi)
	{
		mid = (lo + hi) >> 1;
		if (gFR_COS_TAB_Q[mid] > v)
			lo = mid + 1;
		else
			hi = mid;
	}
	best_idx = lo;
	best_err = (gFR_COS_TAB_Q[best_idx] > v) ? (gFR_COS_TAB_Q[best_idx] - v)
	                                         : (v - gFR_COS_TAB_Q[best_idx]);
	if (best_idx > 0)
	{
		left = gFR_COS_TAB_Q[best_idx - 1] - v;
		if (left < 0) left = -left;
		if (left < best_err) { best_err = left; best_idx = best_idx - 1; }
	}
	if (best_idx < FR_TRIG_TABLE_SIZE)
	{
		right = gFR_COS_TAB_Q[best_idx + 1] - v;
		if (right < 0) right = -right;
		if (right < best_err) { best_err = right; best_idx = best_idx + 1; }
	}

	/* best_idx is in [0, FR_TRIG_TABLE_SIZE]. Convert to degrees:
	 * the table covers [0, 90] degrees in FR_TRIG_TABLE_SIZE steps.
	 * deg = best_idx * 90 / FR_TRIG_TABLE_SIZE.
	 */
	{
		s16 deg = (s16)((best_idx * 90 + (FR_TRIG_TABLE_SIZE >> 1)) / FR_TRIG_TABLE_SIZE);
		return sign ? (s16)(180 - deg) : deg;
	}
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

/* FR_atan2(y, x) — full-circle arctangent, returns degrees as s16.
 *
 * Computes a ratio y/x so the result is independent of the input radix.
 *
 * Algorithm:
 *   1. Special-case the four axis directions.
 *   2. Reduce |y|/|x| to [0,1] (swap so the smaller magnitude is on top).
 *   3. Look up arctan in the table with linear interpolation.
 *   4. Apply quadrant / swap corrections to get the final angle.
 *
 * Range: [-180, 180] degrees. Returns 0 for atan2(0,0) (consistent with
 * IEEE 754 atan2 even though that case is mathematically undefined).
 */
s16 FR_atan2(s32 y, s32 x)
{
	s32 ay, ax, ratio;
	s16 a;

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
		ratio = (s32)(((int64_t)ay << 16) / ax);
		a = fr_atan_unit_q1_deg(ratio);          /* [0..45] */
	}
	else
	{
		ratio = (s32)(((int64_t)ax << 16) / ay);
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
 * `input` is at the given radix. Returns degrees as s16, range [-90, 90].
 */
s16 FR_atan(s32 input, u16 radix)
{
	s32 one = (s32)1 << radix;
	return FR_atan2(input, one);
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
			return FR_OVERFLOW_POS;
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
	mant_log2 = lo + (s32)(((int64_t)(hi - lo) * frac) >> 25);

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
 * FR_printNumD - write a decimal integer with space padding.
 *
 * Equivalent to "%*d" in printf, modulo the return convention.
 *
 *   f       : per-character output function (e.g. putchar). Must not be NULL.
 *   n       : signed integer to print.
 *   pad     : minimum field width; spaces are prepended to reach this width.
 *
 * Returns the number of characters written on success, or -1 if `f` is NULL.
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
			scaled = (u32)(((uint64_t)mag_frac * 10));
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

/*=======================================================
 * Square root and hypot
 *
 * fr_isqrt64 is a private helper implementing the digit-by-digit
 * ("shift-and-subtract") integer square root. The algorithm is bit-exact
 * (returns floor(sqrt(n))) and uses no division. Iteration count is fixed:
 * 32 iterations.
 */
static u32 fr_isqrt64(uint64_t n)
{
	uint64_t root = 0;
	uint64_t bit  = (uint64_t)1 << 62;
	while (bit > n) bit >>= 2;
	while (bit != 0)
	{
		uint64_t trial = root + bit;
		if (n >= trial)
		{
			n -= trial;
			root = (root >> 1) + bit;
		}
		else
		{
			root >>= 1;
		}
		bit >>= 2;
	}
	return (u32)root;
}

/*=======================================================
 * FR_sqrt - fixed-radix square root.
 *
 *   input  : value at radix `radix`. Must be >= 0.
 *   radix  : fractional bits of input AND result.
 *   return : sqrt(input) at radix `radix`, or FR_DOMAIN_ERROR if input < 0.
 *
 * Math: sqrt(input_fp / 2^r) at radix r is
 *   result_fp = sqrt(input_fp / 2^r) * 2^r = sqrt(input_fp * 2^r)
 * so we compute isqrt(input_fp << radix) on a 64-bit accumulator. This
 * works for any input that fits in s32 and any radix in [0, 30].
 *
 * Precision: bit-exact floor(sqrt). Worst-case absolute error is < 1
 * LSB at the requested radix (the truncation of the floor operation).
 * Always non-negative for non-negative input. Result is monotone in
 * input.
 *
 * Saturation: input < 0 returns FR_DOMAIN_ERROR (= INT32_MIN). Caller
 * can test `result == FR_DOMAIN_ERROR` to detect domain errors.
 *
 * Side effects: none. Pure function.
 */
s32 FR_sqrt(s32 input, u16 radix)
{
	uint64_t n;

	if (input < 0)
		return FR_DOMAIN_ERROR;
	if (input == 0)
		return 0;

	n = (uint64_t)(u32)input << radix;
	return (s32)fr_isqrt64(n);
}

/*=======================================================
 * FR_hypot - sqrt(x*x + y*y) without intermediate overflow.
 *
 *   x, y   : values at radix `radix`
 *   radix  : fractional bits of inputs AND result
 *   return : sqrt(x*x + y*y) at radix `radix`.
 *
 * Math: x*x + y*y is naturally at radix 2*radix; isqrt of a 2r-radix
 * value yields an r-radix result, so no extra shifting is needed. The
 * u64 accumulator can hold (INT32_MAX^2)*2 = ~2^63, so (x*x + y*y) never
 * overflows for any s32 inputs.
 *
 * Precision: bit-exact floor(hypot). Worst-case absolute error < 1 LSB
 * at the requested radix.
 *
 * Side effects: none. Pure function.
 */
s32 FR_hypot(s32 x, s32 y, u16 radix)
{
	uint64_t xx = (uint64_t)((int64_t)x * (int64_t)x);
	uint64_t yy = (uint64_t)((int64_t)y * (int64_t)y);
	(void)radix; /* the 2*radix in xx+yy cancels with isqrt's halving */
	return (s32)fr_isqrt64(xx + yy);
}

/*=======================================================
 * Wave generators — synth-style fixed-shape waveforms.
 *
 * All wave functions take a u16 BAM phase in [0, 65535] (a full cycle)
 * and return s16 in s0.15 format, clamped to [-32767, +32767] to match
 * the trig amplitude convention used by fr_cos_bam / fr_sin_bam.
 *
 * Use FR_HZ2BAM_INC(hz, sample_rate) to compute a phase increment for
 * a given output frequency, then accumulate it (mod 2^16) per sample.
 *
 * Side effects: pure functions (except fr_wave_noise which advances a
 * caller-provided LFSR state pointer).
 */

/* fr_wave_sqr - 50%-duty square wave.
 * phase < pi (BAM<0x8000) → +full; phase >= pi → -full.
 */
s16 fr_wave_sqr(u16 phase)
{
	return (phase < 0x8000) ? (s16)32767 : (s16)-32767;
}

/* fr_wave_pwm - variable-duty pulse.
 * `duty` is the BAM threshold: phase < duty → high, else low.
 *   duty = 0      → always low
 *   duty = 0x8000 → 50% duty (same as fr_wave_sqr)
 *   duty = 0xffff → high almost everywhere (one BAM step low)
 */
s16 fr_wave_pwm(u16 phase, u16 duty)
{
	return (phase < duty) ? (s16)32767 : (s16)-32767;
}

/* fr_wave_saw - rising sawtooth.
 * Linear ramp from -32767 (just after phase=0) to +32767 (at phase=0xffff),
 * passing through 0 at phase=0x8000. The single boundary case phase=0
 * (which would naturally produce -32768) is clamped to -32767 to keep the
 * amplitude symmetric.
 */
s16 fr_wave_saw(u16 phase)
{
	s32 v = (s32)phase - (s32)0x8000;
	if (v < -32767) v = -32767;
	return (s16)v;
}

/* fr_wave_tri - symmetric triangle.
 * Four linear segments:
 *   Q1 [0, 0x4000)  : rising  0 → +peak
 *   Q2 [0x4000, 0x8000): falling +peak → 0
 *   Q3 [0x8000, 0xc000): falling 0 → -peak
 *   Q4 [0xc000, 0x10000): rising  -peak → 0
 * Peaks are clamped to +/-32767 (the natural unclamped formula gives
 * +/-32768 at the exact peak BAM).
 */
s16 fr_wave_tri(u16 phase)
{
	s32 t;
	if (phase < 0x8000)
	{
		/* First half: 0 -> +peak -> 0 */
		if (phase < 0x4000)
			t = (s32)phase << 1;          /* 0 .. 0x7ffe */
		else
			t = (s32)(0x8000 - phase) << 1; /* 0x8000 .. 2 */
		if (t > 32767) t = 32767;
		return (s16)t;
	}
	else
	{
		/* Second half: 0 -> -peak -> 0 */
		if (phase < 0xc000)
			t = (s32)(phase - 0x8000) << 1; /* 0 .. 0x7ffe */
		else
			t = (s32)(0x10000 - phase) << 1;/* 0x8000 .. 2 */
		if (t > 32767) t = 32767;
		return (s16)-t;
	}
}

/* fr_wave_tri_morph - variable-symmetry triangle.
 *
 *   phase       : u16 BAM
 *   break_point : u16 BAM where the wave reaches its positive peak.
 *
 * Going from 0 to +peak in [0, break_point), then from +peak back to 0
 * in [break_point, 0xffff]. The result is a triangle whose rising and
 * falling slopes can differ.
 *
 *   break_point = 0x8000  → symmetric triangle
 *   break_point = 0xffff  → rising sawtooth (instant fall)
 *   break_point = 0x0001  → falling sawtooth (instant rise)
 *   break_point = 0       → degenerate; treated as 1 to avoid div-by-zero
 *
 * Note that this version returns values in [0, 32767] only (not bipolar).
 * Caller can subtract 16384 and double if a bipolar version is desired.
 *
 * Costs: one 32-bit divide per sample. On Cortex-M3+ this is ~10-20
 * cycles. On 8051 / MSP430 this is much slower; pre-compute slopes if
 * those targets matter to you.
 */
s16 fr_wave_tri_morph(u16 phase, u16 break_point)
{
	u32 t;
	if (break_point == 0)
		break_point = 1;
	if (phase < break_point)
	{
		/* rising: 0 at phase=0, 32767 at phase=break_point */
		t = ((u32)phase * 32767UL) / (u32)break_point;
	}
	else
	{
		/* falling: 32767 at phase=break_point, 0 at phase=0xffff */
		u32 span = (u32)0xffff - (u32)break_point;
		if (span == 0)
			return 32767;
		t = ((u32)((u32)0xffff - (u32)phase) * 32767UL) / span;
	}
	if (t > 32767) t = 32767;
	return (s16)t;
}

/* fr_wave_noise - LFSR-based pseudorandom noise.
 *
 *   state : pointer to a u32 the caller maintains. Initial value must
 *           be non-zero (zero is a fixed point of the LFSR). A common
 *           seed is 0xACE1u or any other non-zero constant.
 *
 * Returns the next s16 sample in s0.15 (full ±32767 range, white-ish).
 * Implementation: 32-bit Galois LFSR with the standard maximal-period
 * tap polynomial 0xD0000001 (period 2^32 - 1 samples).
 *
 * Quality: this is "fast white noise" suitable for synth use. It is NOT
 * cryptographically secure. For better statistical properties (FFT
 * flatness etc.) layer a longer LFSR or use a separate PRNG.
 */
s16 fr_wave_noise(u32 *state)
{
	u32 lsb;
	if (!state)
		return 0;
	lsb = *state & 1u;
	*state >>= 1;
	if (lsb)
		*state ^= 0xD0000001u;
	/* Take the top 16 bits and re-bias to s16 range, clamp to ±32767. */
	{
		s32 v = (s32)((*state >> 16) & 0xffffu) - 32768;
		if (v < -32767) v = -32767;
		return (s16)v;
	}
}

/*=======================================================
 * ADSR envelope generator
 *
 * Linear-segment Attack-Decay-Sustain-Release envelope. State is held
 * in caller-allocated fr_adsr_t struct (no global state, no malloc).
 *
 * Lifecycle:
 *   1. Caller allocates an fr_adsr_t (stack or static).
 *   2. fr_adsr_init() once per patch with attack/decay/release durations
 *      in samples and a sustain level in s0.15.
 *   3. fr_adsr_trigger() on note-on. Output rises 0 -> peak over `atk`
 *      samples, falls peak -> sustain over `dec` samples, then holds.
 *   4. fr_adsr_release() on note-off. Output falls current -> 0 over a
 *      time controlled by the release rate (rate, not duration: the
 *      time depends on where in the envelope we are).
 *   5. fr_adsr_step() once per audio sample to read the current value.
 *
 * Internal precision: levels are stored as s32 in s1.30 format so even
 * very long envelopes (e.g. 48000-sample attack at 48 kHz = 1 second)
 * have a non-zero per-sample increment. Output is converted to s0.15.
 *
 * Saturation: the envelope state machine is self-clamping; level cannot
 * escape [0, 1<<30]. Output is in [0, 32767].
 */

#define FR_ADSR_PEAK_S130 ((s32)1 << 30)

void fr_adsr_init(fr_adsr_t *env,
                  u32 attack_samples,
                  u32 decay_samples,
                  s16 sustain_level_s015,
                  u32 release_samples)
{
	if (!env)
		return;
	env->state   = FR_ADSR_IDLE;
	env->level   = 0;

	/* sustain_level_s015 is s16 so its upper bound (32767) is already the
	 * type's max; only the lower bound needs an explicit clamp. */
	if (sustain_level_s015 < 0)
		sustain_level_s015 = 0;
	/* Convert s0.15 -> s1.30 by shifting left 15. */
	env->sustain = (s32)sustain_level_s015 << 15;

	env->attack_inc  = (attack_samples  > 0)
	    ? (s32)(FR_ADSR_PEAK_S130 / attack_samples)
	    : FR_ADSR_PEAK_S130;
	env->decay_dec   = (decay_samples   > 0)
	    ? (s32)((FR_ADSR_PEAK_S130 - env->sustain) / decay_samples)
	    : (FR_ADSR_PEAK_S130 - env->sustain);
	env->release_dec = (release_samples > 0)
	    ? (s32)(FR_ADSR_PEAK_S130 / release_samples)
	    : FR_ADSR_PEAK_S130;
}

void fr_adsr_trigger(fr_adsr_t *env)
{
	if (!env)
		return;
	env->state = FR_ADSR_ATTACK;
	env->level = 0;
}

void fr_adsr_release(fr_adsr_t *env)
{
	if (!env)
		return;
	env->state = FR_ADSR_RELEASE;
}

s16 fr_adsr_step(fr_adsr_t *env)
{
	if (!env)
		return 0;
	switch (env->state)
	{
	case FR_ADSR_ATTACK:
		env->level += env->attack_inc;
		if (env->level >= FR_ADSR_PEAK_S130)
		{
			env->level = FR_ADSR_PEAK_S130;
			env->state = FR_ADSR_DECAY;
		}
		break;
	case FR_ADSR_DECAY:
		env->level -= env->decay_dec;
		if (env->level <= env->sustain)
		{
			env->level = env->sustain;
			env->state = FR_ADSR_SUSTAIN;
		}
		break;
	case FR_ADSR_SUSTAIN:
		env->level = env->sustain;
		break;
	case FR_ADSR_RELEASE:
		env->level -= env->release_dec;
		if (env->level <= 0)
		{
			env->level = 0;
			env->state = FR_ADSR_IDLE;
		}
		break;
	case FR_ADSR_IDLE:
	default:
		env->level = 0;
		break;
	}
	/* s1.30 -> s0.15: shift right 15. Clamp for safety. */
	{
		s32 out = env->level >> 15;
		if (out < 0) out = 0;
		if (out > 32767) out = 32767;
		return (s16)out;
	}
}
