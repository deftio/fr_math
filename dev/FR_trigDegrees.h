/**
 *	@FR_trigDegrees.h - header definition file for fixed radix math routines
 *                        trigonometry functions, with degrees 
 *                        sin(), cos(), tan(),  
 *                        asin(), acos(), atan(),
 *    Naming convention:
 *    FR_<function><units><optional: integer>  
 *    example:
 *    FR_tanDegreesI() --> FixedRadix, tangent, degrees, Integer input
 *		
 *	@copy Copyright (C) <2001-2015>  <M. A. Chatterjee>
 *    @author M A Chatterjee <deftio [at] deftio [dot] com>
 *	@version 1.01 M. A. Chatterjee, cleaned up naming
 *
 *  This file contains integer math settable fixed point radix math routines for
 *  use on systems in which floating point is not desired or unavailable.
 *  naming cleaned up in 2012, but otherwise collected from random progs I've
 *  written in the last 15 or so years.
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

#ifndef __FR_trigDegrees_h__
#define __FR_trigDegrees_h__

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef __FR_Platform_Defs_H__
#include "FR_defs.h"
#endif


/* Convert to double, this is for debug only and WILL NOT compile under many embedded systems.
 * Fixed Radix to Floating Point Double Conversion
 * since this is a MACRO it will not be compiled or instantiated unless it is actually called in code. */
#define FR2D_DEG(x,r) ((double)(((double)(x))/((double)(1<<(r)))))

/******************************************************
 * Useful Constants
 *
 * FR_kXXXX "k" denotes constant to help when reading macros used in source code
 * FR_krXXX "r" == reciprocal e.g. 1/XXX 
 * As these are MACROS, they take only compiled code space if actually used.
 * Consts here calculated by multiply the natural base10 value by (1<<FR_kPREC)
 */
#define FR_kPREC       (16)         /* bits of precision in constants listed below */
#define FR_kE          (178145)     /* 2.718281828459 */	
#define FR_krE         (24109)      /* 0.367879441171 */
#define FR_kPI         (205887)	    /* 3.141592653589 */
#define FR_krPI        (20861)	    /* 0.318309886183 */
#define FR_kDEG2RAD    (1144)		/* 0.017453292519 */
#define FR_kRAD2DEG    (3754936)	/*57.295779513082 */

#define FR_kQ2RAD      (102944)     /* 1.570796326794 */
#define FR_kRAD2Q      (41722)      /* 0.636619772367 */

/*log2 to ln conversions (see MACROS) */
#define FR_kLOG2E	   (94548)	    /* 1.442695040890 */
#define FR_krLOG2E	   (45426)	    /* 0.693147180560 */

/*log2 to log10 conversions (see MACROS) */
#define FR_kLOG2_10     (217706)	/* 3.32192809489 */
#define FR_krLOG2_10    (19728)  	/* 0.30102999566 */

//common sqrts
#define FR_kSQRT2       (92682)	    /* 1.414213562373 */
#define FR_krSQRT2      (46341)	    /* 0.707106781186 */
#define FR_kSQRT3       (113512)	/* 1.732050807568 */
#define FR_krSQRT3      (37837)	    /* 0.577350269189 */
#define FR_kSQRT5       (146543)	/* 2.236067977599 */
#define FR_krSQRT5      (29309)	    /* 0.447213595499 */
#define FR_kSQRT10      (207243) 	/* 3.162277660168 */
#define FR_krSQRT10     (20724) 	/* 0.316227766016 */

/* ===============================================
 * Fixed Point Math Operations
 * note: FR_FIXMUL32u is for positive 32bit numbers only 
 * for signed and signed saturated use FR_FixMuls, FR_FixMulSat below
 */
 
#define FR_FIXMUL32u(x,y)	(					\
	(((x>>16)*(y>>16))<<16)+					\
	((x>>16)*(y&0xffff))+						\
	((y>>16)*(x&0xffff))+						\
	((((x&0xffff)*(y&0xffff))))					\
	)

/*===============================================
 * Arithmetic operations
 */
s32 FR_FixMuls(s32 x, s32 y);	// mul signed/unsigned NOT Saturated
s32 FR_FixMulSat(s32 x, s32 y); // mul signed/unsigned AND Saturated
s32 FR_FixAddSat(s32 x, s32 y); // add signed/unsigned AND Saturated


/*================================================
 * Constants used in Trig tables, definitions
 */
#define FR_TRIG_PREC  	(15)
#define FR_TRIG_MASK	((1<<FR_TRIG_PREC)-1)
#define FR_TRIG_MAXVAL 	(FR_TRIG_MASK)
#define FR_TRIG_MINVAL 	(-FR_TRIG_MASK)

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
#define FR_SMUL10(x)	(((x)<<3)+(((x)<<1)))
#define FR_SDIV10(x)	(((x)>>3)-((x)>>5)+((x)>>7)-((x)>>9)+((x)>>11)) 

/* scale by 1/log2(e)  0.693147180560 used for converting log2() to ln()  */
#define FR_SrLOG2E(x)   (((x)>>1)+((x)>>2)-((x)>>3)+((x)>>4)+((x)>>7)-((x)>>9)-((x)>>12)+((x)>>15))
/* scale by log2(e)    1.442695040889 used for converting pow2() to exp() */
#define FR_SLOG2E(x)	((x)+((x)>>1)-((x)>>4)+((x)>>8)+((x)>>10)+((x)>>12)+((x)>>14))

/* scale by 1/log2(10) 0.30102999566 used for converting log2() to log10  */
#define FR_SrLOG2_10(x) (((x)>>2)+((x)>>4)-((x)>>6)+((x)>>7)-((x)>>8)+((x)>>12))
/* scale by log2(10)   3.32192809489 used for converting pow2() to pow10 */
#define FR_SLOG2_10(x)  (((x)<<1)+(x)+((x)>>2)+((x)>>4)+((x)>>7)+((x)>>10)+((x)>>11)+((x)>>13))

/* TRIG Conversion macros (using bit shifting)
 * Convert degrees <--> radians <--> quadrants <--> degrees
 * no multiply (may reduce chances of overflow in certain circumstances)
 * works on all int types and radixes (pure ints will have trunc err)
 * radians   = 2*pi per revolution
 * degrees   = 360  per revolution
 * quadrants = 4 per revolution
 * freq      = 1 per revolution (1 freq == 4 quadrants)
 */
#define FR_DEG2RAD(x) (((x)<<6)-((x)<<3)+(x)+((x)>>2)+((x>>4)-((x)>>6))-((x)>>10))
#define FR_RAD2DEG(x) (((x)>>6)+((x)>>9)-((x)>>13))

#define FR_RAD2Q(x)   (((x)>>1)+((x)>>3)+((x)>>7)+((x)>>8)-((x)>>14))
#define FR_Q2RAD(x)   ((x)+((x)>>1)+((x)>>4)+((x)>>7)+((x)>>11))

#define FR_DEG2Q(x)   (((x)>>6)-((x)>>8)-((x)>>11)-((x)>>13))
#define FR_Q2DEG(x)   (((x)<<6)+((x)<<4)+((x)<<3)+((x)<<1))

/* sin, cos with integer input (degrees), s.15 result                  */
s16 FR_CosI(s16 deg); 
s16 FR_SinI(s16 deg);

/* tan with integer input precision in degrees, returns, s15.16 result */
s32 FR_TanI(s16 deg);

/* Fixed radix (interpolated) input (in degrees), s.15 result */
s16 FR_Cos(s16 deg,u16 radix);
s16 FR_Sin(s16 deg,u16 radix);

/* Fixed radix tan returns fixed  s15.16 result result (interpolated) */
s32 FR_Tan(s16 deg,u16 radix);

/* Inverse trig (output in degrees) */
s16 FR_acos(s32 input, u16 radix);
s16 FR_asin(s32 input, u16 radix);
s16 FR_atan(s32 input, u16 radix);

/* Logarithms */
#define FR_LOG2MIN ((-32767)<<16) /* returned instead of "negative infinity" */

s32 FR_log2( s32 input, u16 radix, u16 output_radix);
s32 FR_ln(   s32 input, u16 radix, u16 output_radix);
s32 FR_log10(s32 input, u16 radix, u16 output_radix);


/* Power */
s32 FR_pow2( s32 input, u16 radix);
#define FR_EXP(input,radix) (FR_pow2(FR_SLOG2E(input),radix))
#define FR_POW10(input,radix) (FR_pow2(FR_SLOG2_10(input),radix))
/*
s32 FR_exp(  s32 input, u16 radix);
s32 FR_pow10(s32 input, u16 radix); 	
*/
#ifdef __cplusplus

} // extern "C"
#endif

#endif /* __FR_MathRoutines_h__ */
