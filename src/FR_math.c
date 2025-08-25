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

/*=======================================================
 * FR_FixMuls (x*y signed NOT saturated mul)
 * User must keep track of new radix point after using this function
 */

s32 FR_FixMuls(s32 x, s32 y) /* signed not saturated */
{
	s32 z, sign = (x < 0) ? (y > 0) : (y < 0);

	x = FR_ABS(x);
	y = FR_ABS(y);
	z = FR_FIXMUL32u(x, y);

	return sign ? -z : z;
}

/*=======================================================
 * FR_FixMulSat (x*y saturated multiply)
 * User must keep track of new radix point after using this function
 */

s32 FR_FixMulSat(s32 x, s32 y)
{
	s32 z, h, l, m1, m2;
	int sign = (((x < 0) && (y > 0)) || ((x > 0) && (y < 0))) ? 1 : 0;

	x = FR_ABS(x);
	y = FR_ABS(y);
	h = ((x >> 16) * (y >> 16));
	m1 = ((x >> 16) * (y & 0xffff));
	m2 = ((y >> 16) * (x & 0xffff));
	l = (((x & 0xffff) * (y & 0xffff)));

	z = (h << 16) + m1 + m2 + l;

	if (h & 0xffff8000) /* TODO: make better saturation check -- this might be a bug */
		z = 0x7fffffff;
	else
	{
		z = (h << 16) + m1;
		if (z < 0)
			z = 0x7fffffff;
		else
		{
			z += m2;
			if (z < 0)
				z = 0x7fffffff;
			else
			{
				z += l;
				if (z < 0)
					z = 0x7fffffff;
			}
		}
	}
	return sign ? -z : z;
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
	if (270 == deg)
		return -(FR_TRIG_MAXVAL << FR_TRIG_PREC);

	if (deg >= 0)
		return deg <= (90) ? FR_TN(deg) : -FR_TN(180 - deg);
	else
		return deg >= (-90) ? -FR_TN(-deg) : FR_TN(180 + deg);
}
/* Tan with s15.16 result with fixed radix input precision, returns interpolated s15.16 result
 */

s32 FR_Tan(s16 deg, u16 radix)
{
	s16 i, j;
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
s16 FR_atan2(s32 y, s32 x, u16 radix)
{
	int q;
	if (0 == x)
		return y < 0 ? FR_TRIG_MINVAL : FR_TRIG_MAXVAL;

	q = (y >= 0) ? ((x >= 0) ? 0 : 1) : ((x >= 0) ? 3 : 2);
	return q;
}

/* Raise x to the xth power
 */

#define FR_kPOW2COEF_PREC (4)
u16 static const gPOW2_ICOEF[17] = {
	0, 2902, 5932, 9096, 12400, 15850, 19454, 23216, 27146,
	31249, 35534, 40009, 44682, 49562, 54658, 59979, 65535}; /* 16bit prec */

/* Accuracy vs space tradeoff:
 * can save some space by comment out above coef and using smaller table here.
 *
#define FR_kPOW2COEF_PREC	(3)
u16 static const gPOW2_ICOEF[9] = {0,5932,12400,19454,27146,35534,44682,54658,65535}; // 16bit prec
 */

// TODO: keep prec up... should be input# of bits + 14 (includes interp error etc..)
//  pow2: 383 bytes of 32bit x86 code under gcc cygwin with tables counted (and before output radix stuff added)
// e.g. outputradix = input+14 bits of prec so
//  if input > 30 output = MAXINTu32
//  if input+14 < 20 output radix = 30-input
//  if input+14 < 16 ouptut radix = 16 (e.g. don't just slam the radix point way to the left we don't have that precision anyway.
s32 FR_pow2(s32 input, u16 radix)
{
	s32 flr, frac, k, j, sc;
	flr = FR_INT(input, radix);
	frac = FR_FRACS(input, radix, FR_kPOW2COEF_PREC);
	k = (s32)(gPOW2_ICOEF[frac]);
	j = (s32)(gPOW2_ICOEF[frac + 1]);
	sc = ((j - k) >> 8) + ((j - k) >> 10) + ((j - k) >> 11); /* slope correction */
	frac = ((FR_FRACS(input, radix, 15)) - (FR_FRACS(frac, FR_kPOW2COEF_PREC, 15))) << FR_kPOW2COEF_PREC;
	if (frac <= (1 << (15 - 1)))
		sc = FR_INTERP(0, sc, frac, 14);
	else
		sc = FR_INTERP(sc, 0, frac - (1 << (15 - 1)), 14);

	k = FR_INTERP(k, j, frac, 15) - sc;

	if (flr >= 0) /* positive powers of 2 */
	{
		k = FR_CHRDX((k << flr), 16, radix);
		j = (1 << flr) << radix;
	}
	else /* negative powers of 2 */
	{
		flr = -flr;
		k = FR_CHRDX((k >> flr), 16, radix);
		j = (radix - flr) >= 0 ? 1 << (radix - flr) : 0;
	}
	return j + k;
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

/*log2 returns FR_LOG2MIN if input <= 0 */
s32 FR_log2(s32 input, u16 radix, u16 output_radix)
{
	s32 h = 16; //,frac = FR_FRAC(input,radix);
	while (h >>= 1)
	{
		if (input > (1 << h))
		{
			input >>= h;
		} // i+=h; input>>=h
	}

	return input <= 0 ? FR_LOG2MIN : FR_CHRDX(input, radix, output_radix);
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

int FR_printNumF(int (*f)(char), s32 n, int radix, int pad, int prec)
{
	int t = 10, s = 0, r = FR_FRAC(n, radix);
	n = FR_INT(n, radix);
	if (f)
	{
		if (n < 0)
		{
			n = -n, s = 1;
			pad--;
		}
		while ((n / t) > 0)
		{
			t *= 10;
			pad--;
		}
		while (pad-- > 0)
		{
			f(' ');
		}
		if (s)
			f('-');
		while (t >= 10)
		{
			t /= 10;
			f((char)((n / t) % 10) + '0');
		}
		if (prec || r)
		{
			f('.');
			s = 1 << (radix);
			if (r)
			{
				while (s)
				{
					r *= 10;
					s /= 10;
				}
			}
			r >>= radix;
			// r++;
			t = 1;
			while ((r / t) > 0)
			{
				t *= 10;
			}
			while ((t >= 10) && (prec))
			{
				t /= 10;
				prec--;
				f((char)((r / t) % 10) + '0');
			}
			while (prec-- > 0)
			{
				f('0');
			}
		}
		return 0;
	}
	return -1;
}
/***************************************
 FR_printNumD write out a decimal integer with space padding
  equiavlent ot %d in printf family
  int num = 12
  e.g. printf("%4d",num) ==> "  12"

 */
int FR_printNumD(int (*f)(char), int n, int pad)
{
	int t = 10, s = 0;
	if (f)
	{
		if (n < 0)
		{
			n = -n, s = 1;
			pad--;
		}
		while ((n / t) > 0)
		{
			t *= 10;
			pad--;
		}
		while (pad-- > 0)
		{
			f(' ');
		}
		if (s)
			f('-');
		while (t >= 10)
		{
			t /= 10;
			f((char)((n / t) % 10) + '0');
		}
		return FR_S_OK;
	}
	return FR_E_FAIL;
}
/***************************************
 FR_printNumH write out a integer as hex
 if (showPrefix == true )
	print "0x"
 else
	//no prefix printed
 */

int FR_printNumH(int (*f)(char), int n, int showPrefix)
{
	int d, x = ((sizeof(int)) << 1) - 1;
	if (f)
	{
		if (showPrefix)
		{
			f('0');
			f('x');
		}

		do
		{
			d = (n >> (x << 2)) & 0xf;
			d = (d > 9) ? (d - 0xa + 'a') : (d + '0');
			f(d);
		} while (x--);
		return FR_S_OK;
	}
	return FR_E_FAIL;
}
