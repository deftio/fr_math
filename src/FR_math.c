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

#ifndef FR_NO_STDINT
#include <stdint.h>
#endif

/*=======================================================
 * Trig lookup tables (inlined — no separate header needed)
 *
 * Sine quadrant table: 129 entries covering [0, pi/2] in u0.15 format.
 * The final entry is 32768, so the s15.16 crest (<< 1) is exact ±1.0.
 * Define FR_TRIG_S16_SAFE when compiling this file to end the table at
 * 32767 instead. The crest is then 65534, and (s16)(crest >> 1) is
 * ±32767 rather than a wrapped -32768. The cardinal path returns this
 * same endpoint, so 90° matches the table in either build.
 * Tangent octant table: 65 entries covering [0, pi/4] in u0.15 format.
 */

#define FR_TRIG_TABLE_BITS  (7)
#define FR_TRIG_TABLE_SIZE  ((1 << FR_TRIG_TABLE_BITS) + 1)

#define FR_TRIG_FRAC_BITS   (14 - FR_TRIG_TABLE_BITS)
#define FR_TRIG_FRAC_MAX    (1 << FR_TRIG_FRAC_BITS)
#define FR_TRIG_FRAC_MASK   (FR_TRIG_FRAC_MAX - 1)
#define FR_TRIG_FRAC_HALF   (FR_TRIG_FRAC_MAX >> 1)
#define FR_TRIG_QUADRANT    (1 << 14)

static const unsigned short gFR_SIN_TAB_Q[FR_TRIG_TABLE_SIZE] = {
        0,   402,   804,  1206,  1608,  2009,  2411,  2811,
     3212,  3612,  4011,  4410,  4808,  5205,  5602,  5998,
     6393,  6787,  7180,  7571,  7962,  8351,  8740,  9127,
     9512,  9896, 10279, 10660, 11039, 11417, 11793, 12167,
    12540, 12910, 13279, 13646, 14010, 14373, 14733, 15091,
    15447, 15800, 16151, 16500, 16846, 17190, 17531, 17869,
    18205, 18538, 18868, 19195, 19520, 19841, 20160, 20475,
    20788, 21097, 21403, 21706, 22006, 22302, 22595, 22884,
    23170, 23453, 23732, 24008, 24279, 24548, 24812, 25073,
    25330, 25583, 25833, 26078, 26320, 26557, 26791, 27020,
    27246, 27467, 27684, 27897, 28106, 28311, 28511, 28707,
    28899, 29086, 29269, 29448, 29622, 29792, 29957, 30118,
    30274, 30425, 30572, 30715, 30853, 30986, 31114, 31238,
    31357, 31471, 31581, 31686, 31786, 31881, 31972, 32058,
    32138, 32214, 32286, 32352, 32413, 32470, 32522, 32568,
    32610, 32647, 32679, 32706, 32729, 32746, 32758, 32766,
#ifdef FR_TRIG_S16_SAFE
    32767
#else
    32768
#endif
};

/* s15.16 crest, taken from the table endpoint so the lookup and the
 * cardinal returns stay the same value. Default: 65536. With
 * FR_TRIG_S16_SAFE: 65534. */
#define FR_SIN_PEAK ((s32)gFR_SIN_TAB_Q[FR_TRIG_TABLE_SIZE - 1] << 1)

#define FR_TAN_TABLE_BITS  (6)
#define FR_TAN_TABLE_SIZE  ((1 << FR_TAN_TABLE_BITS) + 1)
#define FR_TAN_FRAC_BITS   (13 - FR_TAN_TABLE_BITS)
#define FR_TAN_FRAC_MAX    (1 << FR_TAN_FRAC_BITS)
#define FR_TAN_FRAC_MASK   (FR_TAN_FRAC_MAX - 1)
#define FR_TAN_FRAC_HALF   (FR_TAN_FRAC_MAX >> 1)
#define FR_TAN_OCTANT      (1 << 13)

static const unsigned short gFR_TAN_TAB_O[FR_TAN_TABLE_SIZE] = {
        0,   402,   804,  1207,  1610,  2013,  2417,  2822,
     3227,  3634,  4042,  4450,  4861,  5272,  5686,  6101,
     6518,  6937,  7358,  7782,  8208,  8637,  9068,  9503,
     9940, 10381, 10825, 11273, 11725, 12180, 12640, 13104,
    13573, 14046, 14525, 15009, 15498, 15993, 16494, 17001,
    17515, 18035, 18563, 19098, 19640, 20191, 20750, 21318,
    21895, 22481, 23078, 23685, 24302, 24931, 25572, 26226,
    26892, 27572, 28266, 28975, 29699, 30440, 31198, 31973,
    32768
};

/*=======================================================
 * Full-precision radian/degree → BAM conversion helpers
 *
 * rad_to_bam_full(r) returns a full s32 BAM value where:
 *   upper 16 bits = integer BAM (the u16 table index)
 *   lower 16 bits = sub-BAM fractional part
 * Input r must already be normalized to radix 16 and reduced to [-pi, pi].
 *
 * The shift terms match FR_RAD2BAM (10 terms, ~21-bit accuracy) but are
 * reordered so intermediate sums stay within s32 for |r| <= pi at r16.
 */
static s32 rad_to_bam_full(s32 r)
{
    /* 10 terms: 65536/(2*pi) ≈ 10430.37835...
     * 2^13 + 2^11 + 2^7 + 2^6 - 2 + 0.5 - 0.125 + 2^-8 - 2^-11 - 2^-14
     * = 10430.378357 (~21-bit accuracy)
     * Terms reordered: interleave negatives early to keep all intermediate
     * sums within s32 for |r| <= pi at r16 (max result ≈ 2^31 - 4K). */
    return (r<<13)-(r<<1)+(r<<11)-(r>>3)+(r<<7)+(r<<6)+(r>>1)+(r>>8)-(r>>11)-(r>>14);
}

#ifndef FR_LEAN
/* deg_to_bam_full(d) — same idea for degrees.
 * Input d must already be normalized to radix 16 and reduced to [-90, 90).
 * Returns full s32 BAM (upper 16 = integer BAM, lower 16 = sub-BAM).
 * 7 terms, ~18-bit accuracy matching FR_DEG2BAM. */
static s32 deg_to_bam_full(s32 d)
{
    return (d<<7)+(d<<6)-(d<<3)-(d<<1)+(d>>5)+(d>>6)-(d>>9);
}
#endif

/* Normalize a fixed-radix value to radix 16. Below radix 16 the caller
 * must make sure the shifted value fits (see rad_mag_r16, deg_to_r16). */
static s32 normalize_to_r16(s32 val, u16 radix)
{
    return (radix > 16) ? (val >> (radix - 16))
         : (radix < 16) ? (val << (16 - radix))
         : val;
}

/* 2^64 / (2*pi): turns per radian as a u0.64 fraction. (x * this) mod
 * 2^64 is frac(x / 2*pi) for an integer x; shifted right by the radix it
 * serves every radix 0..31 with error < 2^-32 turn for |x| < 2^32. */
#define FR_TURNS_PER_RAD_Q64 0x28BE60DB9391054AULL
#define FR_TWO_PI_R24        105414357u           /* 2*pi * 2^24 */

/* Exact-period reduction: |angle| (mag, at radix) mod 2*pi, returned at
 * r16 in [0, 2*pi]. Works for any magnitude, including 2^31 (INT32_MIN)
 * and inputs whose r16 value does not fit in s32. Two multiplies and no
 * division; the period is 2*pi to 64 bits, so large angles do not pick
 * up the drift of an integer period. */
static u32 rad_to_turns(u32 mag, u16 radix)        /* frac(mag/2pi) * 2^32 */
{
    return (u32)(((u64)mag * (FR_TURNS_PER_RAD_Q64 >> radix)) >> 32);
}
static s32 turns_to_r16(u32 turns)                 /* [0, 2pi] at r16 */
{
    return (s32)(((u64)turns * FR_TWO_PI_R24 + (1ULL << 39)) >> 40);
}

/* |rad| at radix -> r16 radians reduced to [0, 2*pi].
 * Inputs whose r16 value is at most 4*pi take the original path
 * (normalize, |.|, subtract 2*pi once) and give the same result as
 * before. Larger ones, and those whose r16 value would overflow s32 at
 * radix < 16, are reduced exactly through rad_to_turns().
 * If turns is not NULL, *turns also receives |rad| mod 2*pi in 2^-32
 * turns, reduced exactly from the unreduced value: finer than r16 and
 * free of the integer-2*pi step (fr_tan uses it near the poles). */
static s32 rad_mag_r16x(s32 rad, u16 radix, u32 *turns)
{
    const s32 two_pi = FR_TWO_PI(16);  /* 411775 */
    u32 m, t;
    if (radix >= 16) {
        s32 r = normalize_to_r16(rad, radix);
        m = (r < 0) ? 0u - (u32)r : (u32)r;
    } else {
        u32 a = (rad < 0) ? 0u - (u32)rad : (u32)rad;
        if (a > ((u32)(two_pi << 1) >> (16 - radix))) {
            t = rad_to_turns(a, radix);
            if (turns) *turns = t;
            return turns_to_r16(t);
        }
        m = a << (16 - radix);
    }
    if (turns) *turns = rad_to_turns(m, 16);
    if (m > (u32)(two_pi << 1))
        return turns_to_r16(rad_to_turns(m, 16));
    if (m > (u32)two_pi)
        m -= (u32)two_pi;
    return (s32)m;
}
static s32 rad_mag_r16(s32 rad, u16 radix)
{
    return rad_mag_r16x(rad, radix, (u32 *)0);
}

#ifndef FR_LEAN
/* Degrees at radix -> r16. Below radix 16 a value whose r16 form would
 * overflow s32 is first reduced mod 360 at the caller's radix, which is
 * exact (360 << radix is an integer); other values are unchanged. */
static s32 deg_to_r16(s32 deg, u16 radix)
{
    if (radix < 16) {
        u32 lim = (u32)1 << (15 + radix);          /* |deg| < lim fits */
        if ((u32)deg + lim >= (lim << 1))
            deg %= (s32)360 << radix;
    }
    return normalize_to_r16(deg, radix);
}

/* Integer degrees -> BAM for the radix-0 paths: FR_DEG2BAM_I is exact
 * for |deg| < 2^18; reduce mod 360 (exact) outside that. */
static u16 deg_i_to_bam(s32 deg)
{
    if (deg >= ((s32)1 << 18) || deg <= -((s32)1 << 18))
        deg %= 360;
    return FR_DEG2BAM_I(deg);
}

/* Degrees at r16 (non-negative, < 2^20) -> 2^-32 turns, rounded. */
#define FR_TURNS_PER_DEG16_Q32 781874935307ULL     /* 2^48 / 360 */
static u32 deg16_to_turns(u32 d)
{
    return (u32)(((u64)d * FR_TURNS_PER_DEG16_Q32 + ((u64)1 << 31)) >> 32);
}
#endif

/* cot(e) as s15.16 for a small angle e > 0 given in 2^-32 turns, i.e.
 * the distance to a tan pole. cot e = 1/e - e/3 - e^3/45 - ...; the
 * first two terms leave a relative error under e^4/45 (2e-5 at 10 deg).
 * 1/e = 2^48 / (2*pi * t) at s15.16 is formed from 2^40 / t' (two u32
 * divisions, exact) times 2^8/(2*pi), with t' = t scaled below 2^24; no
 * 64-bit divide. Saturates to FR_TRIG_MAXVAL when the result does not
 * fit (t <= 20860, ~0.0017 deg). Valid for t < 2^27 (11.25 deg). */
#define FR_COT_TURNS_K26 2734261102u               /* 2^8/(2pi) * 2^26 */
static s32 cot_turns(u32 t)
{
    u32 k = 0, q, rem, tt;
    if (t <= 20860u)
        return FR_TRIG_MAXVAL;
    while ((t >> k) >= ((u32)1 << 24))
        k++;
    tt  = t >> k;
    q   = 0xFFFFFFFFu / tt;
    rem = 0xFFFFFFFFu - q * tt + 1u;           /* 2^32 = q*tt + rem    */
    if (rem >= tt) { q++; rem -= tt; }
    q = (q << 8) + ((rem << 8) / tt);          /* floor(2^40 / tt)     */
    q = (u32)(((u64)q * FR_COT_TURNS_K26) >> (26 + k));
    /* - e/3 at s15.16 = t * 2pi/(3 * 2^16) = (t/2^12) * 34315/2^18 */
    return (s32)(q - (((t >> 12) * 34315u) >> 18));
}

/* Near-pole windows for the tan family: inside them tan is computed as
 * cot of the exact distance to the pole instead of through a u16 BAM,
 * whose rounding error the reciprocal amplifies by about 1/e. 10 deg. */
#define FR_TAN_POLE_TURNS 119304647                /* 10 deg in 2^-32 turns */
#define FR_TAN_POLE_BAM   1820u                    /* 10 deg in BAM      */
#define FR_TAN_POLE_DEG16 ((s32)10 << 16)          /* 10 deg at r16      */


/* rad_r16_to_bam — convert radian (at r16) in [0, 2π) to u16 BAM.
 * Uses quadrant decomposition to keep rad_to_bam_full in its safe
 * [-π/2, π/2) range, mirroring the approach in fr_deg_to_bam. */
static u16 rad_r16_to_bam(s32 r)
{
    const s32 half_pi       = FR_HALF_PI(16);       /* 102944 */
    const s32 three_half_pi = FR_THREE_HALF_PI(16); /* 308831 */
    const s32 pi            = FR_PI(16);             /* 205887 */
    const s32 two_pi        = FR_TWO_PI(16);         /* 411775 */

    u16 offset = 0;
    if (r >= half_pi && r < three_half_pi) {
        r -= pi;
        offset = 0x8000u;
    } else if (r >= three_half_pi) {
        r -= two_pi;
        /* r is now in [-π/2, 0), no offset needed (u16 wraps naturally) */
    }
    return (u16)(offset + (u16)((rad_to_bam_full(r) + ((s32)1 << 15)) >> 16));
}

/* (rad_r16_to_bam32 removed — sub-BAM interpolation approach abandoned) */

/* fr_rad_to_bam — overflow-safe radian to u16 BAM conversion.
 * Normalizes to r16, reduces to [0, 2π), uses quadrant decomposition. */
u16 fr_rad_to_bam(s32 rad, u16 radix)
{
    /* Reduce |rad| to [0, 2π]; a negative angle maps to 2π - that. */
    s32 r = rad_mag_r16(rad, radix);
    if (rad < 0 && r != 0)
        r = FR_TWO_PI(16) - r;
    return rad_r16_to_bam(r);
}

#ifndef FR_LEAN
/* fr_deg_to_bam — overflow-safe degree to u16 BAM conversion.
 * Normalizes to r16, reduces to [-90, 90) with quadrant offset. */
u16 fr_deg_to_bam(s32 deg, u16 radix)
{
    s32 d = deg_to_r16(deg, radix);

    /* Reduce to [-180, 180) */
    if (d >= FR_D360_R16 || d < -FR_D360_R16) {
        s32 n = d / FR_D360_R16;
        d -= n * FR_D360_R16;
    }
    if (d >=  FR_D180_R16) d -= FR_D360_R16;
    if (d <  -FR_D180_R16) d += FR_D360_R16;

    /* Reduce to [-90, 90) with BAM quadrant offset */
    u16 offset = 0;
    if (d >= FR_D90_R16)      { d -= FR_D180_R16; offset = 32768; }
    else if (d < -FR_D90_R16) { d += FR_D180_R16; offset = 32768; }

    return (u16)(offset + (u16)((deg_to_bam_full(d) + ((s32)1 << 15)) >> 16));
}
#endif

/*=======================================================
 * BAM-native trig: fr_sin_bam, fr_cos_bam, fr_cos, fr_sin, fr_tan
 *
 * Internal model: every angle is reduced to a u16 BAM value. The top 2 bits
 * select the quadrant, the bottom 14 bits are the in-quadrant position. Odd
 * quadrants (1, 3) reverse the in-quadrant index so the table is always read
 * in the same direction.
 *
 * The table is a 129-entry SINE quadrant (ascending: 0 at index 0, full
 * scale at index 128). After mirroring, small full_pos → small output (near zero),
 * which enables a cheap small-angle approximation: sin(θ) ≈ θ for angles
 * below one table step (~0.7°). This eliminates table quantization error
 * in the region where it matters most.
 *
 * Sign rule: quadrants 2 and 3 negate the result.
 * Mirror rule: quadrants 1 and 3 flip the in-quadrant position.
 */
s32 fr_sin_bam(u16 bam)
{
	u32 q   = ((u32)bam >> 14) & 0x3;              /* top 2 bits = quadrant */
	u32 inq = (u32)bam & (FR_TRIG_QUADRANT - 1);  /* bottom 14 bits        */

	/* Exact cardinal angles. Crest is the table endpoint (FR_SIN_PEAK),
	 * so 90° matches the last table entry in both crest modes. */
	if (inq == 0) {
		if (q == 0 || q == 2) return 0;            /*   0° or 180° → 0    */
		return (q == 1) ? FR_SIN_PEAK : -FR_SIN_PEAK; /* 90° / 270°       */
	}

	/* Odd quadrants mirror: read table from the far end */
	if (q == 1 || q == 3)
		inq = FR_TRIG_QUADRANT - inq;

	s32 v;

	/* Small-angle approximation: sin(θ) ≈ θ for inq < 128 (one table step).
	 * θ_rad = inq * (π/2) / 16384.  Output = θ * 65536 = inq * FR_kQ2RAD / 16384.
	 * Max inq=127: 127 * 102944 / 16384 = 798. Error: θ³/6 < 3e-7 << 1 LSB. */
	if (inq < FR_TRIG_FRAC_MAX) {
		v = (s32)(((u32)inq * 102944u + 8192u) >> 14);
	} else {
		/* Table lookup with 7-bit interpolation fraction */
		u32 idx  = inq >> FR_TRIG_FRAC_BITS;
		u32 frac = inq &  FR_TRIG_FRAC_MASK;
		s32 lo = (s32)gFR_SIN_TAB_Q[idx];
		s32 hi = (s32)gFR_SIN_TAB_Q[idx + 1];
		v = lo + (((hi - lo) * (s32)frac + FR_TRIG_FRAC_HALF) >> FR_TRIG_FRAC_BITS);
		v <<= 1;   /* u0.15 → s15.16 */
	}

	return (q >= 2) ? -v : v;
}

s32 fr_cos_bam(u16 bam)
{
	/* cos(x) = sin(x + pi/2) = sin(bam + 16384). u16 wraparound is free. */
	return fr_sin_bam((u16)(bam + FR_BAM_QUADRANT));
}

s32 fr_cos(s32 rad, u16 radix)
{
	if (rad == 0) return FR_SIN_PEAK;
	s32 r = rad_mag_r16(rad, radix);        /* cos is even: |rad| */
	/* Near π/2 or 3π/2 (cos=0 crossings): cos(π/2+δ) = -sin(δ) ≈ -δ,
	 * cos(3π/2+δ) = sin(δ) ≈ δ. */
	s32 delta = r - FR_HALF_PI(16);
	if (delta >= -256 && delta <= 256)
		return -delta;
	delta = r - FR_THREE_HALF_PI(16);
	if (delta >= -256 && delta <= 256)
		return delta;
	return fr_cos_bam(fr_rad_to_bam(rad, radix));
}

s32 fr_sin(s32 rad, u16 radix)
{
	if (rad == 0) return 0;
	/* sin is odd: work with |rad| */
	s32 sign = (rad < 0) ? -1 : 1;
	s32 r = rad_mag_r16(rad, radix);
	/* Near 0 after reduction: sin(δ) ≈ δ */
	if (r < 256) {
		s32 v = r;
		return (sign < 0) ? -v : v;
	}
	/* Near π: sin(π + δ) = -sin(δ) ≈ -δ */
	s32 delta = r - FR_PI(16);
	if (delta >= -256 && delta <= 256) {
		s32 v = -delta;
		return (sign < 0) ? -v : v;
	}
	/* Near 2π: sin(2π - δ) = -sin(δ) ≈ -δ, but δ = 2π - r */
	delta = FR_TWO_PI(16) - r;
	if (delta >= 0 && delta < 256) {
		s32 v = -delta;
		return (sign < 0) ? -v : v;
	}
	/* Main path: reduce to [-π, π], convert to u16 BAM, table lookup */
	if (r > FR_PI(16)) r -= FR_TWO_PI(16);
	u16 bam = (u16)((rad_to_bam_full(r) + ((s32)1 << 15)) >> 16);
	s32 v = fr_sin_bam(bam);
	return (sign < 0) ? -v : v;
}

#ifndef FR_LEAN
/*=======================================================
 * BAM-native tangent: fr_tan_bam
 *
 * Uses a 65-entry octant table (gFR_TAN_TAB_O) for the first octant
 * [0, 45°] and the reciprocal identity tan(x) = 1/tan(90°-x) for the
 * second octant (45°, 90°), and cot of the exact distance within 10° of
 * a pole. Result is s15.16 with saturation at the poles.
 *
 * No 64-bit intermediates. One 32-bit division only in the >45° path.
 */
s32 fr_tan_bam(u16 bam)
{
	u32 q   = ((u32)bam >> 14) & 0x3;          /* quadrant (top 2 bits) */
	u32 inq = (u32)bam & 0x3FFFu;              /* in-quadrant (14 bits) */
	s32 sign = 1;
	u32 idx, frac;
	s32 lo, hi, raw;

	/* Exact zeros: bam lands exactly on 0° or 180° */
	if (inq == 0 && (q == 0 || q == 2))
		return 0;

	/* Poles: bam lands exactly on 90° or 270° */
	if (inq == 0 && (q == 1 || q == 3))
		return (q == 1) ? FR_TRIG_MAXVAL : -FR_TRIG_MAXVAL;

	/* Q1 (90°..180°) and Q3 (270°..360°): reflect and negate */
	if (q == 1 || q == 3) {
		inq = 0x4000u - inq;
		sign = -1;
	}

	/* Now inq is in (0, 0x4000) = (0°, 90°) exclusive.
	 * Split into first octant [0, 45°) and second octant [45°, 90°). */
	if (inq < FR_TAN_OCTANT) {
		/* First octant: direct table lookup + lerp.
		 * inq is 13 bits; top FR_TAN_TABLE_BITS index the table,
		 * bottom FR_TAN_FRAC_BITS drive interpolation. */
		idx  = inq >> FR_TAN_FRAC_BITS;
		frac = inq &  FR_TAN_FRAC_MASK;
		lo = (s32)gFR_TAN_TAB_O[idx];
		hi = (s32)gFR_TAN_TAB_O[idx + 1];
		raw = lo + (((hi - lo) * (s32)frac + FR_TAN_FRAC_HALF) >> FR_TAN_FRAC_BITS);

		if (raw < 0x40) {
			/* Near zero: redo interpolation with 4 extra bits of
			 * precision to reduce rounding error when result is small. */
			s32 lo4 = (s32)gFR_TAN_TAB_O[idx] << 4;
			s32 hi4 = (s32)gFR_TAN_TAB_O[idx + 1] << 4;
			raw = lo4 + (((hi4 - lo4) * (s32)frac + FR_TAN_FRAC_HALF) >> FR_TAN_FRAC_BITS);
			raw = (raw + 4) >> 3;        /* u0.19 → s15.16 with rounding */
		} else {
			raw <<= 1;                   /* u0.15 → s15.16              */
		}
	} else {
		/* Second octant: tan(x) = 1 / tan(90° - x).
		 * complement is in (0, 0x2000] = (0°, 45°]. */
		u32 comp = 0x4000u - inq;

		if (comp <= FR_TAN_POLE_BAM) {
			/* Within 10° of the pole: cot(e) = 1/e - e/3 with
			 * e = comp * 2π/65536 exactly, one u32 division:
			 * 65536/e = 2^32/(2π) / comp; e/3 = comp * 2π/3 at s15.16.
			 * The table reciprocal would amplify interpolation error
			 * by 1/e here. */
			raw = (s32)(683565276u / comp - ((comp * 2145u) >> 10));
		} else {
			/* Look up tan(complement) from the table */
			idx  = comp >> FR_TAN_FRAC_BITS;
			frac = comp &  FR_TAN_FRAC_MASK;
			lo = (s32)gFR_TAN_TAB_O[idx];
			/* comp == 45° gives idx == FR_TAN_TABLE_SIZE - 1 with frac == 0:
			 * that entry is exact, and idx + 1 would read past the table. */
			hi = (frac != 0) ? (s32)gFR_TAN_TAB_O[idx + 1] : lo;
			raw = lo + (((hi - lo) * (s32)frac + FR_TAN_FRAC_HALF) >> FR_TAN_FRAC_BITS);
			/* comp > 10° keeps raw >= tan(10°) in u0.15 (5778): no
			 * near-zero divisor. */
			raw = (s32)(0x80000000u / (u32)raw);
		}
	}

	return (sign < 0) ? -raw : raw;
}
#endif /* FR_LEAN */

/* fr_tan — radian-input tangent.
 *
 * Normalize to [0, 2π], extract quadrant sign, convert rad→u16 BAM,
 * then do direct octant table lookup + interpolation inline.
 * Small-angle bypass at zero crossings: tan(x) ≈ x.
 * Within 10° of a pole: cot of the exact distance to the pole
 * (cot_turns), to avoid BAM quantization error amplified by the
 * reciprocal. */
s32 fr_tan(s32 rad, u16 radix)
{
	if (rad == 0) return 0;

	/* tan(-x) = -tan(x): extract sign, work with |rad| */
	s32 sign = (rad < 0) ? -1 : 1;
	u32 turns;
	s32 r = rad_mag_r16x(rad, radix, &turns);

	/* Small-angle bypass at zero crossings: tan(δ) ≈ δ */
	if (r < 256)
		return (sign < 0) ? -r : r;
	{
		s32 delta = r - FR_PI(16);
		if (delta >= -256 && delta <= 256)
			return (sign < 0) ? -delta : delta;
	}
	{
		s32 delta = FR_TWO_PI(16) - r;
		if (delta >= 0 && delta < 256)
			return (sign < 0) ? delta : -delta;
	}

	/* Near-pole bypass: within 10° of π/2 or 3π/2,
	 * tan(pole ∓ δ) = ±cot(δ). The angle is taken in 2^-32 turns, where
	 * the poles are exactly 2^30 and 3·2^30 (rad_mag_r16x reduces the
	 * input to turns exactly). See cot_turns(). */
	{
		s32 t = (s32)(turns - 0x40000000u);
		if (t < -FR_TAN_POLE_TURNS || t > FR_TAN_POLE_TURNS)
			t = (s32)(turns - 0xC0000000u);
		if (t >= -FR_TAN_POLE_TURNS && t <= FR_TAN_POLE_TURNS) {
			/* before the pole (t < 0): → +∞; past it: → -∞ */
			s32 v = cot_turns((t < 0) ? 0u - (u32)t : (u32)t);
			if (t >= 0) v = -v;
			return (sign < 0) ? -v : v;
		}
	}

	/* Convert radian to u16 BAM */
	u16 bam = rad_r16_to_bam(r);

	/* Decompose BAM into quadrant + in-quadrant */
	u32 q   = ((u32)bam >> 14) & 0x3;
	u32 inq = (u32)bam & 0x3FFFu;
	s32 tsign = 1;  /* tan sign from quadrant */

	/* Exact zeros: bam lands on 0° or 180° */
	if (inq == 0 && (q == 0 || q == 2))
		return 0;

	/* Q1/Q3: reflect and negate */
	if (q == 1 || q == 3) {
		inq = 0x4000u - inq;
		tsign = -1;
	}

	/* Octant table lookup + interpolation (same logic as fr_tan_bam) */
	u32 idx, frac;
	s32 raw;

	if (inq < FR_TAN_OCTANT) {
		/* First octant [0°, 45°): direct lookup */
		idx  = inq >> FR_TAN_FRAC_BITS;
		frac = inq &  FR_TAN_FRAC_MASK;
		s32 lo = (s32)gFR_TAN_TAB_O[idx];
		s32 hi = (s32)gFR_TAN_TAB_O[idx + 1];
		raw = lo + (((hi - lo) * (s32)frac + FR_TAN_FRAC_HALF) >> FR_TAN_FRAC_BITS);

		if (raw < 0x40) {
			s32 lo4 = lo << 4;
			s32 hi4 = hi << 4;
			raw = lo4 + (((hi4 - lo4) * (s32)frac + FR_TAN_FRAC_HALF) >> FR_TAN_FRAC_BITS);
			raw = (raw + 4) >> 3;
		} else {
			raw <<= 1;
		}
	} else {
		/* Second octant [45°, 90°): reciprocal identity */
		u32 comp = 0x4000u - inq;
		idx  = comp >> FR_TAN_FRAC_BITS;
		frac = comp &  FR_TAN_FRAC_MASK;
		s32 lo = (s32)gFR_TAN_TAB_O[idx];
		/* comp == 45° gives idx == FR_TAN_TABLE_SIZE - 1 with frac == 0:
		 * that entry is exact, and idx + 1 would read past the table. */
		s32 hi = (frac != 0) ? (s32)gFR_TAN_TAB_O[idx + 1] : lo;
		raw = lo + (((hi - lo) * (s32)frac + FR_TAN_FRAC_HALF) >> FR_TAN_FRAC_BITS);
		/* Within 10° of a pole returned earlier (cot path), so comp is
		 * at least ~10° and raw >= tan(10°) in u0.15: no small divisor. */
		raw = (s32)(0x80000000u / (u32)raw);
	}

	/* Combine quadrant sign and input sign */
	s32 v = (tsign < 0) ? -raw : raw;
	return (sign < 0) ? -v : v;
}

#ifndef FR_LEAN
/*=======================================================
 * Degree-input trig: convert to u16 BAM via fr_deg_to_bam, then
 * call the BAM-native functions. Cardinal zeros are exact; the crest
 * is the quadrant-table endpoint (FR_SIN_PEAK).
 */

s32 fr_cos_deg(s32 deg, u16 radix)
{
	if (radix == 0) return fr_cos_bam(deg_i_to_bam(deg));
	/* cos is even. INT32_MIN has no positive counterpart in s32; it is
	 * never a cardinal angle and fr_deg_to_bam() reduces negative input
	 * itself, so it is evaluated as is. */
	if (deg < 0 && deg != (s32)(-0x7fffffff - 1)) deg = -deg;
	/* Exact cardinal angles */
	s32 frac_mask = ((s32)1 << radix) - 1;
	if ((deg & frac_mask) == 0) {
		s32 rem = (deg >> radix) % 360;
		if (rem == 0)   return  FR_SIN_PEAK;
		if (rem == 90)  return  0;
		if (rem == 180) return -FR_SIN_PEAK;
		if (rem == 270) return  0;
	}
	/* Near 90° or 270° (cos=0 crossings): cos(90+δ) = -sin(δ) ≈ -δ·π/180,
	 * cos(270+δ) = sin(δ) ≈ δ·π/180. Avoids BAM rounding error at zero. */
	s32 d = deg_to_r16(deg, radix);
	/* Reduce to [0, 360°); d < 0 only for deg == INT32_MIN (see above). */
	d %= FR_D360_R16;
	if (d < 0) d += FR_D360_R16;
	{
		const s32 DEG_THRESH = 14000; /* ~0.21° at r16 */
		s32 delta = d - FR_D90_R16;
		if (delta >= -DEG_THRESH && delta <= DEG_THRESH) {
			s32 dr = (s32)(((s64)delta * FR_kDEG2RAD + ((s64)1 << 15)) >> 16);
			return -dr;
		}
		delta = d - (FR_D90_R16 + FR_D180_R16);
		if (delta >= -DEG_THRESH && delta <= DEG_THRESH) {
			s32 dr = (s32)(((s64)delta * FR_kDEG2RAD + ((s64)1 << 15)) >> 16);
			return dr;
		}
	}
	return fr_cos_bam(fr_deg_to_bam(deg, radix));
}

s32 fr_sin_deg(s32 deg, u16 radix)
{
	if (radix == 0) return fr_sin_bam(deg_i_to_bam(deg));
	s32 sign = 1;
	/* sin is odd. INT32_MIN has no positive counterpart in s32; it is
	 * never a cardinal angle and fr_deg_to_bam() reduces negative input
	 * itself, so it is evaluated as is (sign included). */
	if (deg < 0 && deg != (s32)(-0x7fffffff - 1)) { deg = -deg; sign = -1; }
	/* Exact cardinal angles */
	s32 frac_mask = ((s32)1 << radix) - 1;
	if ((deg & frac_mask) == 0) {
		s32 rem = (deg >> radix) % 360;
		if (rem == 0)   return  0;
		if (rem == 90)  return (sign < 0) ? -FR_SIN_PEAK :  FR_SIN_PEAK;
		if (rem == 180) return  0;
		if (rem == 270) return (sign < 0) ?  FR_SIN_PEAK : -FR_SIN_PEAK;
	}
	s32 v = fr_sin_bam(fr_deg_to_bam(deg, radix));
	return (sign < 0) ? -v : v;
}

s32 FR_TanI(s32 deg)
{
	/* Exact pole: deg mod 180 == ±90. Sign matches input sign. */
	s32 rem = deg % 180;
	if (rem == 90 || rem == -90)
		return (deg > 0) ? FR_TRIG_MAXVAL : -FR_TRIG_MAXVAL;
	{
		/* Within 10° of a pole: ±cot of the exact distance (see
		 * fr_tan_deg). a in [0, 180), e = a - 90. */
		s32 a = (rem < 0) ? -rem : rem;
		s32 e = a - 90;
		if (e >= -10 && e <= 10) {
			s32 v = cot_turns(deg16_to_turns((u32)((e < 0) ? -e : e) << 16));
			if (e > 0) v = -v;                /* past the pole */
			return (rem < 0) ? -v : v;
		}
	}
	/* tan has period 180°, and FR_DEG2BAM_I(rem) is FR_DEG2BAM_I(deg)
	 * shifted by a multiple of 0x8000: same result, no overflow. */
	return fr_tan_bam(FR_DEG2BAM_I(rem));
}

s32 fr_tan_deg(s32 deg, u16 radix)
{
	if (radix == 0) return FR_TanI(deg);
	s32 deg_orig = deg;
	/* Normalize to [0, 360°) at caller radix. 360 << radix fits in s32
	 * only up to radix 22. Above that |deg| < 256° already, so only a
	 * negative angle needs +360°, which the caller radix cannot hold: it
	 * is applied to the integer degrees of the cardinal check and to d at
	 * r16 below instead, and fr_deg_to_bam() reduces negative input
	 * itself. (% rather than negating, so INT32_MIN is safe.) */
	if (radix <= 22) {
		s32 d360 = 360 << radix;
		deg %= d360;
		if (deg < 0) deg += d360;
	}
	/* Exact cardinal angles */
	s32 frac_mask = ((s32)1 << radix) - 1;
	if ((deg & frac_mask) == 0) {
		s32 ideg = deg >> radix;
		if (ideg < 0) ideg += 360;          /* radix > 22, negative deg */
		if (ideg == 0 || ideg == 180) return 0;
		if (ideg == 90 || ideg == 270)
			return (deg_orig >= 0) ? FR_TRIG_MAXVAL : -FR_TRIG_MAXVAL;
	}
	/* Near 0° or 180° (tan=0 crossings): tan(δ) ≈ δ in radians */
	s32 d = normalize_to_r16(deg, radix);
	if (d < 0) d += FR_D360_R16;          /* radix > 22, negative deg */
	{
		const s32 DEG_THRESH = 14000; /* ~0.21° at r16 */
		s32 delta;
		/* Near 0° */
		if (d < DEG_THRESH) {
			s32 up = d << 8;
			return (FR_DEG2RAD(up) + (1 << 7)) >> 8;
		}
		/* Near 180° */
		delta = d - FR_D180_R16;
		if (delta >= -DEG_THRESH && delta <= DEG_THRESH) {
			s32 up = delta << 8;
			return (FR_DEG2RAD(up) + (1 << 7)) >> 8;
		}
		/* Near 360° */
		delta = FR_D360_R16 - d;
		if (delta >= 0 && delta < DEG_THRESH) {
			s32 up = delta << 8;
			return -((FR_DEG2RAD(up) + (1 << 7)) >> 8);
		}
	}
	/* Within 10° of 90° or 270°: tan(pole ∓ e) = ±cot(e) from the exact
	 * degree distance. Through a u16 BAM, the input's BAM rounding
	 * (up to 0.0027°) is amplified by about 1/e near the pole. An
	 * exact pole only reaches here at radix > 16 (truncated to r16);
	 * it saturates with the sign of the side the input is on. */
	{
		s32 e = d - FR_D90_R16;
		if (e < -FR_TAN_POLE_DEG16 || e > FR_TAN_POLE_DEG16)
			e = d - (FR_D90_R16 + FR_D180_R16);
		if (e >= -FR_TAN_POLE_DEG16 && e <= FR_TAN_POLE_DEG16) {
			s32 v = cot_turns(deg16_to_turns((e < 0) ? 0u - (u32)e : (u32)e));
			return (e < 0) ? v : -v;
		}
	}
	/* Main path: convert to u16 BAM, table lookup */
	return fr_tan_bam(fr_deg_to_bam(deg, radix));
}
#endif /* FR_LEAN */

/*=======================================================
 * FR_FixMuls (x*y signed, NOT saturated, round-to-nearest)
 *
 * Treats x and y as fixed-point values at the same radix r and returns
 * (x*y) >> r at radix r. The user is responsible for tracking the radix
 * point and for guaranteeing the product fits in 32 bits.
 *
 * Adds 0.5 LSB (0x8000) before the shift so the result rounds to
 * nearest instead of truncating toward zero.
 */
s32 FR_FixMuls(s32 x, s32 y)
{
	int64_t v = (int64_t)x * (int64_t)y;
	return (s32)((v + 0x8000) >> 16);
}

/*=======================================================
 * FR_FixMulSat (x*y signed, SATURATED, round-to-nearest)
 *
 * Same semantics as FR_FixMuls but clamps to [INT32_MIN, INT32_MAX] on
 * overflow instead of wrapping. The fixed-point radix is fixed at 16 bits
 * (sM.16 inputs and output). Rounds to nearest (adds 0.5 LSB before shift).
 */
s32 FR_FixMulSat(s32 x, s32 y)
{
	int64_t v = ((int64_t)x * (int64_t)y + 0x8000) >> 16;
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
	/* Sum in unsigned space: signed overflow is undefined behavior, and
	 * optimizers legally delete wrap-detection checks written as x + y. */
	s32 sum = (s32)((u32)x + (u32)y);
	if (x < 0)
	{
		if (y < 0 && sum >= 0)
			return FR_OVERFLOW_NEG;
	}
	else
	{
		/* nonneg + nonneg wraps into [INT32_MIN, -2]; a zero sum (0+0)
		 * is legitimate, so only sum < 0 signals overflow */
		if (y >= 0 && sum < 0)
			return FR_OVERFLOW_POS;
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
/* FR_acos — returns radians at out_radix.
 * Range: [0, pi].  Input is a cosine value at the given radix.
 *
 * Uses the 129-entry sine table in reverse: binary-search the ascending
 * table to find asin(|input|), then acos = pi/2 - asin (with sign handling
 * for the second quadrant).
 */
s32 FR_acos(s32 input, u16 radix, u16 out_radix)
{
	s32 v;
	s16 sign;
	s32 lo, hi, mid;
	s32 idx, d, num, frac;
	s32 input_abs;

	/* Work with absolute value at the caller's radix */
	sign = (s16)((input < 0) ? 1 : 0);

	/* Clamp at the caller's radix */
	{
		/* |input| in u32: -INT32_MIN does not fit in s32. */
		u32 mag = sign ? 0u - (u32)input : (u32)input;
		if (mag >= ((u32)1 << radix))
			return sign ? FR_CHRDX(FR_kPI, FR_kPREC, out_radix) : 0;
		input_abs = (s32)mag;
	}

	v = FR_CHRDX(input_abs, radix, FR_TRIG_PREC); /* |input| at s0.15 */

	/* Small-angle fast path: when cos(θ) is close to 1.0, the sine table
	 * has poor resolution near the top (entries close together).
	 * Use acos(x) ≈ sqrt(2*(1-x)) instead. Threshold: v > sin_tab[121]
	 * means the input is > cos(7*π/256) ≈ 0.9975. */
	if (v > gFR_SIN_TAB_Q[FR_TRIG_TABLE_SIZE - 8])
	{
		s32 one = (s32)1 << radix;
		s32 one_minus_x = one - input_abs;           /* 1-|x| at caller radix */
		s32 two_omx     = one_minus_x << 1;          /* 2(1-|x|) at caller radix */
		s32 rad_native  = FR_sqrt(two_omx, radix);   /* radians at caller radix */
		s32 rad_out     = FR_CHRDX(rad_native, radix, out_radix);
		if (sign)
			rad_out = FR_CHRDX(FR_kPI, FR_kPREC, out_radix) - rad_out;
		return rad_out;
	}

	/* Binary search on the ascending sine table.
	 * gFR_SIN_TAB_Q[0] = 0 (sin 0°). gFR_SIN_TAB_Q[128] is the crest
	 * (32768, or 32767 with FR_TRIG_S16_SAFE).
	 *
	 * Find the first index where table[idx] >= v. */
	lo = 0;
	hi = FR_TRIG_TABLE_SIZE;
	while (lo < hi)
	{
		mid = (lo + hi) >> 1;
		if ((s32)gFR_SIN_TAB_Q[mid] < v)
			lo = mid + 1;
		else
			hi = mid;
	}

	/* lo is now the first index where table[lo] >= v.
	 * The bracketing interval is [lo-1, lo] with table[lo-1] < v <= table[lo].
	 * This gives us the asin angle; acos = pi/2 - asin. */
	idx = lo;
	if (idx <= 0)
	{
		idx = 0;
		frac = 0;
	}
	else if (idx >= FR_TRIG_TABLE_SIZE)
	{
		idx = FR_TRIG_TABLE_SIZE - 1;
		frac = 0;
	}
	else
	{
		/* Interpolate between table[idx-1] and table[idx].
		 * d = table[idx] - table[idx-1]  (>= 0, sin increasing)
		 * num = v - table[idx-1]          (how far past table[idx-1])
		 */
		d   = (s32)gFR_SIN_TAB_Q[idx] - (s32)gFR_SIN_TAB_Q[idx - 1];
		num = v - (s32)gFR_SIN_TAB_Q[idx - 1];
		if (d > 0)
			frac = ((num << FR_TRIG_FRAC_BITS) + (d >> 1)) / d;
		else
			frac = 0;
		idx = idx - 1;
	}

	{
		/* asin_bam is the angle in first-quadrant BAM whose sin = v */
		u16 asin_bam = (u16)(((u32)idx << FR_TRIG_FRAC_BITS) + (u32)frac);
		/* acos = pi/2 - asin (in BAM: quadrant - asin_bam) */
		u16 bam = (u16)(FR_TRIG_QUADRANT - asin_bam);
		if (sign)
			bam = (u16)(FR_BAM_HALF - bam);  /* mirror: pi - angle */
		return FR_CHRDX(FR_Q2RAD(bam), 14, out_radix);
	}
}

/* FR_asin — returns radians at out_radix. Range: [-pi/2, pi/2]. */
s32 FR_asin(s32 input, u16 radix, u16 out_radix)
{
	/* asin(x) = pi/2 - acos(x) */
	s32 half_pi = FR_CHRDX(FR_kQ2RAD, FR_kPREC, out_radix);
	return half_pi - FR_acos(input, radix, out_radix);
}

/* atan(i/64) for i = 0..64, radians in u0.16 (130 bytes).
 * Seeded with round(atan(i/64) * 65536), then each interior entry was
 * nudged by up to a few LSB to minimise the peak error of the linear
 * interpolation below, checked over every one of the 65537 possible
 * ratios (tools/gen_atan_table.py).  The end points stay exact: entry 0
 * is atan(0) = 0 and entry 64 is pi/4, so the two octants meet without a
 * step at 45 degrees.
 * Peak error of the lookup: 1.55 LSB at radix 16 (2.4e-5 rad). */
static const u16 gFR_ATAN_TAB[65] = {
	    0,  1024,  2047,  3070,  4091,  5110,  6126,  7140,
	 8150,  9156, 10158, 11155, 12147, 13134, 14114, 15088,
	16055, 17016, 17968, 18913, 19850, 20779, 21699, 22611,
	23513, 24406, 25290, 26164, 27028, 27883, 28727, 29562,
	30386, 31200, 32004, 32797, 33581, 34353, 35116, 35867,
	36609, 37340, 38061, 38772, 39472, 40162, 40843, 41513,
	42173, 42823, 43464, 44095, 44717, 45329, 45932, 46525,
	47110, 47685, 48252, 48810, 49359, 49900, 50432, 50956,
	51472
};

/* FR_atan2(y, x, out_radix) — full-circle arctangent, returns radians
 * at the specified output radix (s32).
 *
 * Range: [-pi, pi]. Returns 0 for atan2(0,0).
 *
 * Implementation: fold (x, y) into the first octant, so the ratio
 * t = min(|x|,|y|) / max(|x|,|y|) lies in [0, 1]; one u32 divide gives t
 * in u0.16; a 65-entry atan table with linear interpolation gives
 * atan(t); the octant and quadrant are then restored by symmetry:
 *   |y| > |x|   ->  pi/2 - a
 *   x < 0       ->  pi   - a
 *   y < 0       ->  -a
 * The table domain is exactly [0, 1], so no input can index past it and
 * no range clamp is needed.  Worst error about 2 LSB at radix 16.
 */
s32 FR_atan2(s32 y, s32 x, u16 out_radix)
{
	u32 ux, uy, mx, mn, t, idx, fr;
	s32 a, lo, hi;

	/* Axis cases — exact angles, no divide. */
	if (x == 0)
	{
		if (y > 0) return  FR_CHRDX(FR_kQ2RAD, FR_kPREC, out_radix);     /*  pi/2 */
		if (y < 0) return -FR_CHRDX(FR_kQ2RAD, FR_kPREC, out_radix);     /* -pi/2 */
		return 0;
	}
	if (y == 0)
		return (x > 0) ? 0 : FR_CHRDX(FR_kPI, FR_kPREC, out_radix);      /* 0 or pi */

	/* Magnitudes in u32 (-INT32_MIN does not fit in s32). */
	ux = (x < 0) ? 0u - (u32)x : (u32)x;
	uy = (y < 0) ? 0u - (u32)y : (u32)y;
	mx = (ux > uy) ? ux : uy;
	mn = (ux > uy) ? uy : ux;

	/* Scale both by the same power of two (the angle does not change) so
	 * mx sits in [2^15, 2^16): the ratio then keeps 16 bits whatever the
	 * input magnitude, and mn << 16 cannot overflow u32. */
	while (mx >= ((u32)1 << 16)) { mx >>= 1; mn >>= 1; }
	while (mx <  ((u32)1 << 15)) { mx <<= 1; mn <<= 1; }

	/* t = mn / mx in u0.16, rounded; t is in [0, 65536]. */
	t   = ((mn << 16) + (mx >> 1)) / mx;
	idx = t >> 10;                 /* 64 segments of 1024 */
	fr  = t & 1023u;
	if (idx >= 64u)
		a = (s32)gFR_ATAN_TAB[64];  /* t == 1: exactly 45 degrees */
	else
	{
		lo = (s32)gFR_ATAN_TAB[idx];
		hi = (s32)gFR_ATAN_TAB[idx + 1u];
		a  = lo + (((hi - lo) * (s32)fr + 512) >> 10);
	}

	/* Restore octant and quadrant (radians at radix 16). */
	if (uy > ux) a = FR_kQ2RAD - a;     /* pi/2 - a */
	if (x < 0)   a = FR_kPI - a;        /* pi   - a */

	/* Change radix on the magnitude, rounding when narrowing, so that
	 * atan2(-y, x) == -atan2(y, x) exactly at every output radix. */
	if (out_radix < FR_kPREC)
		a = (a + ((s32)1 << (FR_kPREC - 1 - out_radix))) >> (FR_kPREC - out_radix);
	else
		a = FR_CHRDX(a, FR_kPREC, out_radix);
	return (y < 0) ? -a : a;
}

/* FR_atan(input, radix, out_radix) — arctangent of a single argument.
 * Returns radians at out_radix, range [-pi/2, pi/2].
 */
s32 FR_atan(s32 input, u16 radix, u16 out_radix)
{
	s32 one = (s32)1 << radix;
	return FR_atan2(input, one, out_radix);
}

/* 2^f table for f in [0, 1] in 65 entries (64 segments), output in s.16
 * fixed point. Entry i = round(2^(i/64) * 65536).  Size: 260 bytes.
 * Used by FR_pow2 to look up the fractional power of 2 with linear
 * interpolation.
 */
static const u32 gFR_POW2_FRAC_TAB[65] = {
     65536,  66250,  66971,  67700,  68438,  69183,  69936,  70698,
     71468,  72246,  73032,  73828,  74632,  75444,  76266,  77096,
     77936,  78785,  79642,  80510,  81386,  82273,  83169,  84074,
     84990,  85915,  86851,  87796,  88752,  89719,  90696,  91684,
     92682,  93691,  94711,  95743,  96785,  97839,  98905,  99982,
    101070, 102171, 103283, 104408, 105545, 106694, 107856, 109031,
    110218, 111418, 112631, 113858, 115098, 116351, 117618, 118899,
    120194, 121502, 122825, 124163, 125515, 126882, 128263, 129660,
    131072
};

/* FR_pow2(input, radix) — computes 2^(input/2^radix), result at same radix.
 *
 * Algorithm: split input into floor(integer) and fractional part. The
 * fractional part is in [0, 1) by construction (Euclidean / mathematical
 * floor — the fractional part of -2.3 is +0.7, not -0.3). Then
 *   2^(int + frac) = 2^int * 2^frac
 * where 2^frac is looked up from a 65-entry table at radix 16, and 2^int
 * is a shift.
 *
 * Worst-case absolute error: ~1e-5 over [-8, 8] (65-entry table).
 * Linear interpolation leaves a small concavity error in each interval.
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
		/* Negate in u32: -INT32_MIN does not fit in s32. */
		u32 neg = 0u - (u32)input;
		/* 2^-n underflows to 0 once n >= 30 (the sh >= 30 check below);
		 * returning here also keeps -nflr in range for INT32_MIN. */
		if ((neg >> radix) >= 30u)
			return 0;
		s32 nflr = (s32)(neg >> radix);
		s32 nfrc = (s32)(neg & mask);
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

	/* Top 6 bits index the table; bottom 10 are the interpolation fraction. */
	idx     = frac_full >> 10;
	frac_lo = frac_full & ((1L << 10) - 1);
	lo = (s32)gFR_POW2_FRAC_TAB[idx];
	hi = (s32)gFR_POW2_FRAC_TAB[idx + 1];
	mant = lo + (((hi - lo) * frac_lo) >> 10);  /* mant in s.16, in [1.0, 2.0) */

	/* Apply integer shift. mant is at radix 16. We want output at `radix`.
	 * If radix == 16: just shift mant.
	 * Otherwise re-radix mant first.
	 */
	if (flr >= 0)
	{
		/* mant (s.16, < 2^17) * 2^flr, straight to the caller's radix in
		 * one shift: going through r16 first overflowed s32 once
		 * 2^flr >= 2^15 even when the result fits at a lower radix
		 * (FR_pow2(20, 0) returned 0). Same value wherever that did not
		 * overflow. The result saturates when it does not fit in s32. */
		s32 sh = flr + (s32)radix - 16;
		if (sh < 0)
			return mant >> -sh;
		if (sh > 30 || mant > (FR_OVERFLOW_POS >> sh))
			return FR_OVERFLOW_POS;
		return mant << sh;
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

/* log2 mantissa table for m in [1, 2), m = 1 + i/64, returning log2(m)
 * in s.16 fixed point. 65 entries (last is log2(2) = 1.0 = 65536) so the
 * interpolation between idx and idx+1 never reads out of bounds.
 * Size: 260 bytes.  Entry i = round(log2(1 + i/64) * 65536).
 */
static const u32 gFR_LOG2_MANT_TAB[65] = {
        0,  1466,  2909,  4331,  5732,  7112,  8473,  9814,
    11136, 12440, 13727, 14996, 16248, 17484, 18704, 19909,
    21098, 22272, 23433, 24579, 25711, 26830, 27936, 29029,
    30109, 31178, 32234, 33279, 34312, 35334, 36346, 37346,
    38336, 39316, 40286, 41246, 42196, 43137, 44068, 44990,
    45904, 46809, 47705, 48593, 49472, 50344, 51207, 52063,
    52911, 53751, 54584, 55410, 56229, 57040, 57845, 58643,
    59434, 60219, 60997, 61769, 62534, 63294, 64047, 64794,
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
 *      sits at bit 31 (so bits 30..25 are the upper 6 bits of m-1).
 *   3. Look up log2(m) in the 65-entry table with linear interpolation
 *      across the next 24 bits. Result is in s.16.
 *   4. integer_part = (p - radix), then result = (integer_part << 16) +
 *      mantissa_log2.
 *   5. Re-radix to the requested output_radix via FR_CHRDX.
 *
 * Worst-case absolute error: ~6e-5 in log2 units (65-entry table).
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
	 * (m_frac in [0, 2^30)). Index into the 64-entry table is the top 6
	 * bits of m_frac; the lower 24 bits are the interpolation fraction.
	 */
	m -= ((u32)1 << 30);
	idx  = (s32)(m >> 24);                    /* 6 bits  */
	frac = (s32)(m & (((u32)1 << 24) - 1));       /* 24 bits */
	lo = (s32)gFR_LOG2_MANT_TAB[idx];
	hi = (s32)gFR_LOG2_MANT_TAB[idx + 1];
	mant_log2 = lo + (s32)(((int64_t)(hi - lo) * frac) >> 24);

	/* Step 3: assemble. integer_part = p - radix. */
	integer_part = p - (s32)radix;
	result = (integer_part << 16) + mant_log2;

	/* Step 4: re-radix to output_radix. */
	return FR_CHRDX(result, 16, output_radix);
}

s32 FR_ln(s32 input, u16 radix, u16 output_radix)
{
	if (input <= 0)
		return FR_LOG2MIN;      /* as FR_log2 (scaling it gave a non-sentinel) */
	return FR_MULK28(FR_log2(input, radix, output_radix), FR_krLOG2E_28);
}

#ifndef FR_LEAN
s32 FR_log10(s32 input, u16 radix, u16 output_radix)
{
	if (input <= 0)
		return FR_LOG2MIN;      /* as FR_log2 */
	return FR_MULK28(FR_log2(input, radix, output_radix), FR_krLOG2_10_28);
}
#endif

#ifndef FR_NO_PRINT
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
 * FR_numstr — parse a decimal string into a fixed-point value.
 *
 * This is the runtime inverse of FR_printNumF: given a string like
 * "12.34" or "-0.05" and a radix (number of fractional bits), it
 * returns the s32 fixed-point representation.
 *
 * Features:
 *   - Leading whitespace is skipped.
 *   - Optional sign ('+' or '-').
 *   - Up to 9 fractional digits are used (s32 range).
 *   - No malloc, no strtod, no libm.
 *
 * Returns 0 for NULL or empty input.
 */
s32 FR_numstr(const char *s, u16 radix)
{
    static const s32 pow10[10] = {
        1L, 10L, 100L, 1000L, 10000L,
        100000L, 1000000L, 10000000L, 100000000L, 1000000000L
    };
    u64 int_part = 0, mag;
    s32 frac_part = 0;
    int frac_digits = 0, neg = 0;

    if (!s || !*s) return 0;

    while (*s == ' ' || *s == '\t') s++;          /* skip whitespace */
    if (*s == '-') { neg = 1; s++; }              /* sign            */
    else if (*s == '+') { s++; }

    while (*s >= '0' && *s <= '9') {              /* integer part    */
        /* Past 2^31 the value overflows s32 at any radix: stop growing
         * (no u64 wrap) but keep consuming digits. */
        if (int_part <= 0x80000000u)
            int_part = int_part * 10u + (u64)(*s - '0');
        s++;
    }

    if (*s == '.') {                              /* fractional part */
        s++;
        while (*s >= '0' && *s <= '9') {
            if (frac_digits < 9)
                { frac_part = frac_part * 10 + (*s - '0'); frac_digits++; }
            s++;
        }
    }

    /* Saturate out-of-range input, like FR_FixMulSat: the magnitude
     * limit is 2^31 - 1, or 2^31 for a negative value. int_part <= 2^31
     * and radix <= 31 keep the u64 arithmetic below from overflowing. */
    if (int_part > 0x80000000u)
        return neg ? FR_OVERFLOW_NEG : FR_OVERFLOW_POS;
    mag = int_part << radix;
    if (frac_digits > 0)
        mag += ((u64)frac_part << radix) / (u64)pow10[frac_digits];

    if (neg)
        return (mag >= 0x80000000u) ? FR_OVERFLOW_NEG : -(s32)mag;
    return (mag > 0x7fffffffu) ? FR_OVERFLOW_POS : (s32)mag;
}
#endif /* FR_NO_PRINT */

/*=======================================================
 * Square root and hypot
 *
 * fr_isqrt64 is a private helper implementing the digit-by-digit
 * ("shift-and-subtract") integer square root. The core loop computes
 * floor(sqrt(n)), then a final remainder check rounds to nearest.
 * Uses no division. At most 32 iterations.
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
	/* round to nearest: if remainder > root, (root+1)^2 is closer */
	if (n > root)
		root++;
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
 * Precision: round-to-nearest sqrt. Worst-case absolute error is
 * <= 0.5 LSB at the requested radix.
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
 * Precision: round-to-nearest. Worst-case absolute error <= 0.5 LSB
 * at the requested radix.
 *
 * Side effects: none. Pure function.
 */
#ifndef FR_LEAN
s32 FR_hypot(s32 x, s32 y, u16 radix)
{
	uint64_t xx = (uint64_t)((int64_t)x * (int64_t)x);
	uint64_t yy = (uint64_t)((int64_t)y * (int64_t)y);
	u32 d;
	(void)radix; /* the 2*radix in xx+yy cancels with isqrt's halving */
	d = fr_isqrt64(xx + yy);
	/* up to ~1.42 * 2^31: saturate what does not fit (it wrapped) */
	return (d > 0x7FFFFFFFu) ? FR_OVERFLOW_POS : (s32)d;
}
#endif

/*=======================================================
 * FR_hypot_fast8 — 8-segment piecewise-linear magnitude approximation.
 *
 * Shift-only, no multiply, no 64-bit.  Based on the piecewise-linear
 * method described in US Patent 6,567,777 B1 (Chatterjee, expired).
 * Peak error: ~0.10%.
 */
s32 FR_hypot_fast8(s32 x, s32 y)
{
    /* Magnitudes and the segment sums in u32: |INT32_MIN| = 2^31 fits,
     * and the sums (at most ~1.42 * 2^31) cannot wrap. Results that fit
     * in s32 are unchanged; larger ones saturate to FR_OVERFLOW_POS (they
     * used to wrap negative). */
    u32 ax = (x < 0) ? 0u - (u32)x : (u32)x;
    u32 ay = (y < 0) ? 0u - (u32)y : (u32)y;
    u32 hi, lo, d;

    /* hi = max(|x|,|y|), lo = min(|x|,|y|) */
    if (ax > ay) { hi = ax; lo = ay; }
    else         { hi = ay; lo = ax; }

    if (hi == 0) return 0;

    /* 8 piecewise-linear segments: dist ≈ a*hi + b*lo.
     * Boundaries at β = 0.125, 0.25, 0.375, 0.5, 0.625, 0.75, 0.875. */
    if ((hi >> 1) < lo) {
        /* β in (0.5, 1.0] */
        if (lo > hi - (hi >> 2)) {
            /* β in (0.75, 1.0] */
            if (lo > hi - (hi >> 3))                   /* β > 0.875 */
                /* a≈0.7305, b≈0.6836 */
                d = hi - (hi >> 2) - (hi >> 6) - (hi >> 8)
                     + lo - (lo >> 2) - (lo >> 4) - (lo >> 8);
            else                                        /* β in (0.75, 0.875] */
                /* a≈0.7803, b≈0.6262 */
                d = hi - (hi >> 2) + (hi >> 5) - (hi >> 10)
                     + (lo >> 1) + (lo >> 3) + (lo >> 10) + (lo >> 12);
        } else {
            /* β in (0.5, 0.75] */
            if (lo > hi - (hi >> 1) + (hi >> 3))       /* β > 0.625 */
                /* a≈0.8281, b≈0.5630 */
                d = hi - (hi >> 2) + (hi >> 4) + (hi >> 6)
                     + (lo >> 1) + (lo >> 4) + (lo >> 11);
            else                                        /* β in (0.5, 0.625] */
                /* a≈0.8728, b≈0.4893 */
                d = hi - (hi >> 3) - (hi >> 9) - (hi >> 12)
                     + (lo >> 1) - (lo >> 6) + (lo >> 8) + (lo >> 10);
        }
    } else {
        /* β in [0, 0.5] */
        if ((hi >> 2) < lo) {
            /* β in (0.25, 0.5] */
            if ((hi >> 1) - (hi >> 3) < lo)             /* β > 0.375 */
                /* a≈0.9180, b≈0.3984 */
                d = hi - (hi >> 4) - (hi >> 6) - (hi >> 8)
                     + (lo >> 1) - (lo >> 3) + (lo >> 5) - (lo >> 7);
            else                                        /* β in (0.25, 0.375] */
                /* a≈0.9551, b≈0.2988 */
                d = hi - (hi >> 4) + (hi >> 6) + (hi >> 9)
                     + (lo >> 2) + (lo >> 4) - (lo >> 6) + (lo >> 9);
        } else {
            /* β in [0, 0.25] */
            if ((hi >> 3) < lo)                         /* β in (0.125, 0.25] */
                /* a≈0.9839, b≈0.1838 */
                d = hi - (hi >> 6) - (hi >> 11)
                     + (lo >> 2) - (lo >> 4) - (lo >> 8) + (lo >> 12);
            else                                        /* β in [0, 0.125] */
                /* a≈0.9990, b≈0.0620 */
                d = hi - (hi >> 10)
                     + (lo >> 4) - (lo >> 11);
        }
    }

    return (d > 0x7FFFFFFFu) ? FR_OVERFLOW_POS : (s32)d;
}

#ifndef FR_NO_WAVES
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
		t = (u32)(((u32)phase * 32767UL) / (u32)break_point);
	}
	else
	{
		/* falling: 32767 at phase=break_point, 0 at phase=0xffff */
		u32 span = (u32)0xffff - (u32)break_point;
		if (span == 0)
			return 32767;
		t = (u32)(((u32)((u32)0xffff - (u32)phase) * 32767UL) / span);
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
	    ? (s32)((FR_ADSR_PEAK_S130 - env->sustain) / (s32)decay_samples)
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
#endif /* FR_NO_WAVES */
