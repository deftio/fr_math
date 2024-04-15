/**
 *	@file FR_main.cpp - implementation test file for fixed radix cpp library source
 *  This program is intended to be run on a 32bit machine to show how the library works.
 *
 *	@copy Copyright (C) <2001-2012>  <M. A. Chatterjee>
 *  @author M A Chatterjee <deftio [at] deftio [dot] com>
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

#include <stdio.h>
#include <string.h>

#include "FR_defs.h"	/* platform abstraction, constants, status codes */
#include "FR_math.h"	/* header file for fr_math.h                     */
#include "FR_math_2D.h" /* 2D transformations, coordinate transforms     */

// these next 2 lines are only for this demo and not a runtime dependancy on FR_math routines
#include "math.h"
#define R2D (2.0 * 3.14159265358 / 360.0)
#define D2R (360.0 / 2.0 * 3.14159265358)

#define FR_SML(x, y) (fabs(fabs((double)(x)) - fabs((double)(y))))
#define FR_ERR(item, ref) ((fabs((double)ref) > 0.0000001) ? (((((double)item) - ((double)ref))) / ((double)ref)) * (100.0) : FR_SML(item, ref))

int gTestRadians = 0;
int gTestRadianMACROS = 0;
int gTestForwardTrig = 0;
int gTestInvTrig = 0;
int gTestPow2andLog2 = 1;
//===============================================
// Print out the matrix in its pure integer representation
void printMatrixRaw(FR_Matrix2D_CPT *nM)
{
	printf("%8d %8d %8d\n", (int)nM->m00, (int)nM->m01, (int)nM->m02);
	printf("%8d %8d %8d\n", (int)nM->m10, (int)nM->m11, (int)nM->m12);
	printf("%8d %8d %8d\n", 0, 0, 1 << (nM->radix));
}
//===============================================
// Print out the matrix in its floating point equivalent for readability
// note this entire representation won't be available on low end micros
void printMatrixFloat(FR_Matrix2D_CPT *nM)
{
	printf("%10.4f %10.4f %10.4f\n", FR2D(nM->m00, nM->radix), FR2D(nM->m01, nM->radix), FR2D(nM->m02, nM->radix));
	printf("%10.4f %10.4f %10.4f\n", FR2D(nM->m10, nM->radix), FR2D(nM->m11, nM->radix), FR2D(nM->m12, nM->radix));
	printf("%10.4f %10.4f %10.4f\n", 0.0, 0.0, 1.0);
}
//===============================================
// Print out the matrix in both its fixed radix point and floating point equivalents
void printMatrixBoth(FR_Matrix2D_CPT *nM)
{
	printf("%8d %8d %8d   ==> %10.4f %10.4f %10.4f\n", (int)nM->m00, (int)nM->m01, (int)nM->m02, FR2D(nM->m00, nM->radix), FR2D(nM->m01, nM->radix), FR2D(nM->m02, nM->radix));
	printf("%8d %8d %8d   ==> %10.4f %10.4f %10.4f\n", (int)nM->m10, (int)nM->m11, (int)nM->m12, FR2D(nM->m10, nM->radix), FR2D(nM->m11, nM->radix), FR2D(nM->m12, nM->radix));
	printf("%8d %8d %8d   ==> %10.4f %10.4f %10.4f\n", 0, 0, 1 << (nM->radix), 0.0, 0.0, 1.0);
}

//===============================================
// Print out number in integer, floating point forms
void printNumSigned(s32 nNum, u16 radix)
{
	printf("[%8d,%2d] ==> %10.4f", (int)nNum, radix, FR2D(nNum, radix));
}
void printNumUnsigned(u32 nNum, u16 radix)
{
	printf("[%8d,%2d] ==> %10.4f", (int)nNum, radix, FR2D(nNum, radix));
}
void printNumSigned3(s32 n1, u16 r1, s32 n2, u16 r2, s32 n3, u16 r3)
{
	printf("num,radix,float:[%8d,%2d,%10.4f][%8d,%2d,%10.4f][%8d,%2d,%10.4f]\n", (int)n1, (int)r1, FR2D(n1, r1), (int)n2, (int)r2, FR2D(n2, r2), (int)n3, (int)r3, FR2D(n3, r3));
}

typedef struct
{
	double min_err;
	double min_err_val;
	long min_n;
	double min_err_pct;
	long min_err_pct_n;

	double max_err;
	double max_err_val;
	double max_err_pct;
	long max_err_pct_n;
	long max_n;
	double sum_total_err;
	double sum_total_err2;
	long n;
} FR_TEST_ERR;
void err_init(FR_TEST_ERR *e)
{
	e->sum_total_err = 0;
	e->sum_total_err2 = 0;
	e->n = 0;
}
void err_accum(FR_TEST_ERR *e, double val, double ref)
{
	double err = val - ref;
	double epc = FR_ERR(val, ref);
	if (0 == (e->n))
	{
		e->min_err = err;
		e->min_err_val = val;
		e->min_n = 0;
		e->min_err_pct = epc;
		e->min_err_pct_n = 0;
		e->max_err = err;
		e->max_err_val = val;
		e->max_n = 0;
		e->max_err_pct = epc;
		e->max_err_pct_n = 0;
		e->sum_total_err = err;
		e->sum_total_err = (err) * (err);
	}
	else
	{
		if (err < e->min_err)
		{
			e->min_err = err;
			e->min_err_val = val;
			e->min_n = e->n;
		}
		if (epc < e->min_err_pct)
		{
			e->min_err_pct = epc;
			e->min_err_pct_n = e->n;
		}
		if (err > e->max_err)
		{
			e->max_err = val - ref;
			e->max_err_val = val;
			e->max_n = e->n;
		}
		if (epc > e->max_err_pct)
		{
			e->max_err_pct = epc;
			e->max_err_pct_n = e->n;
		}
		e->sum_total_err += err;
		e->sum_total_err2 += (err) * (err);
	}
	e->n += 1;
}
void err_print(FR_TEST_ERR *e, const char *s)
{
	if (e->n > 0)
		printf("%sn[%5ld] min_e: [%11.6f,%11.6f,%5ld] max_e[%11.5f,%11.6f,%5ld] min_pct[%11.5f,%5ld] max_pct [%11.5f,%5ld] tot_e[%13.7f] mse[%13.7f]\n", s, e->n,
			   e->min_err, e->min_err_val, e->min_n, e->max_err, e->max_err_val, e->max_n,
			   e->min_err_pct, e->min_err_pct_n, e->max_err_pct, e->max_err_pct_n,
			   e->sum_total_err, e->sum_total_err2 / (double)(e->n));
	else
		printf("nodata\n");
}
/*
s16 const static gFR_COS_TAB_RAD_S0d15[]={
	32767,  32757,  32727,  32678,  32609,  32520,  32412,  32284,
	32137,  31970,  31785,  31580,  31356,  31113,  30851,  30571,
	30272,  29955,  29621,  29268,  28897,  28510,  28105,  27683,
	27244,  26789,  26318,  25831,  25329,  24811,  24278,  23731,
	23169,  22594,  22004,  21402,  20787,  20159,  19519,  18867,
	18204,  17530,  16845,  16150,  15446,  14732,  14009,  13278,
	12539,  11792,  11038,  10278,   9511,   8739,   7961,   7179,
	 6392,   5601,   4807,   4011,   3211,   2410,   1607,    804,
	 0
  };
#define gFR_COS_TAB_RAD_S0d15_SZ (64)
#define gFR_COS_TAB_RAD_S0d15_SZPREC (6)
#define gFR_COS_TAB_RAD_S0d15_SZMASK (0x3f)
*/
s16 const static gFR_COS_TAB_RAD_S0d15[] = {
	0x007fff, 0x007ffc, 0x007ff5, 0x007fe8, 0x007fd7, 0x007fc1, 0x007fa6, 0x007f86,
	0x007f61, 0x007f37, 0x007f08, 0x007ed4, 0x007e9c, 0x007e5e, 0x007e1c, 0x007dd5,
	0x007d89, 0x007d38, 0x007ce2, 0x007c88, 0x007c29, 0x007bc4, 0x007b5c, 0x007aee,
	0x007a7c, 0x007a04, 0x007989, 0x007908, 0x007883, 0x0077f9, 0x00776b, 0x0076d8,
	0x007640, 0x0075a4, 0x007503, 0x00745e, 0x0073b5, 0x007306, 0x007254, 0x00719d,
	0x0070e1, 0x007022, 0x006f5e, 0x006e95, 0x006dc9, 0x006cf8, 0x006c23, 0x006b4a,
	0x006a6c, 0x00698b, 0x0068a5, 0x0067bc, 0x0066ce, 0x0065dd, 0x0064e7, 0x0063ee,
	0x0062f1, 0x0061f0, 0x0060eb, 0x005fe2, 0x005ed6, 0x005dc6, 0x005cb3, 0x005b9c,
	0x005a81, 0x005963, 0x005842, 0x00571d, 0x0055f4, 0x0054c9, 0x00539a, 0x005268,
	0x005133, 0x004ffa, 0x004ebf, 0x004d80, 0x004c3f, 0x004afa, 0x0049b3, 0x004869,
	0x00471c, 0x0045cc, 0x00447a, 0x004325, 0x0041cd, 0x004073, 0x003f16, 0x003db7,
	0x003c56, 0x003af2, 0x00398c, 0x003824, 0x0036b9, 0x00354d, 0x0033de, 0x00326d,
	0x0030fb, 0x002f86, 0x002e10, 0x002c98, 0x002b1e, 0x0029a3, 0x002826, 0x0026a7,
	0x002527, 0x0023a6, 0x002223, 0x00209f, 0x001f19, 0x001d93, 0x001c0b, 0x001a82,
	0x0018f8, 0x00176d, 0x0015e1, 0x001455, 0x0012c7, 0x001139, 0x000fab, 0x000e1b,
	0x000c8b, 0x000afb, 0x00096a, 0x0007d9, 0x000647, 0x0004b6, 0x000324, 0x000192,
	0x000000};
#define gFR_COS_TAB_RAD_S0d15_SZ (128)
#define gFR_COS_TAB_RAD_s0d15_SZPREC (7)
#define gFR_COS_TAB_RAD_S0d15_SZMASK (0x7f)

#define gFR_COS_INTRP_PREC (5)
#define gFR_COS_INTRP_MASK ((1 << gFR_COS_INTRP_PREC) - 1)
#include <stdio.h>
s16 FR_cos(s32 rad, int prec)
{
	int addr, frac, q, x, y, z, addrc;
	// printf("rad:%6d ",rad);
	rad = ((FR_kRAD2Q >> 1) * rad) >> (FR_kPREC - 1);
	q = (rad >= 0) ? (rad >> prec) & 0x1 : 2 + ((rad >> prec) & 0x1);
	// printf("rad:%6d ",rad);
	// printf("q:%2d ",q);
	addr = FR_CHRDX(rad, prec, gFR_COS_TAB_RAD_s0d15_SZPREC) & gFR_COS_TAB_RAD_S0d15_SZMASK;
	addrc = gFR_COS_TAB_RAD_S0d15_SZ - addr;
	// printf("addr:%6d ",addr);
	frac = FR_CHRDX(rad, prec, gFR_COS_TAB_RAD_s0d15_SZPREC + gFR_COS_INTRP_PREC) & (gFR_COS_INTRP_MASK);
	// printf("frac:%6d\n",rad);

	/*
	if (q < 2)
		return   (0==q) ?  gFR_COS_TAB_RAD_S0d15[addr  ]: -gFR_COS_TAB_RAD_S0d15[addrc];
	else
		return   (2==q) ? -gFR_COS_TAB_RAD_S0d15[addr  ]:  gFR_COS_TAB_RAD_S0d15[addrc];
	*/
	if (q < 2)
	{
		x = (0 == q) ? gFR_COS_TAB_RAD_S0d15[addr] : -gFR_COS_TAB_RAD_S0d15[addrc];
		y = (0 == q) ? gFR_COS_TAB_RAD_S0d15[addr + 1] : -gFR_COS_TAB_RAD_S0d15[addrc - 1];
	}
	else
	{
		x = (2 == q) ? -gFR_COS_TAB_RAD_S0d15[addr] : gFR_COS_TAB_RAD_S0d15[addrc];
		y = (2 == q) ? -gFR_COS_TAB_RAD_S0d15[addr + 1] : gFR_COS_TAB_RAD_S0d15[addrc - 1];
	}
	z = FR_INTERP(x, y, frac, gFR_COS_INTRP_PREC);
	// printf("%5d %5d %5d %5d\n",x,y,frac,z);
	return z;
}
int putSingleChar(char x)
{
	return printf("%c", (char)x);
}
//===============================================
// main program for testing the functions
int main(int argc, char *argv[])
{
	int ret_val = FR_S_OK;
	int i;
	long index;
	FR_TEST_ERR err;
	FR_TEST_ERR err_c, err_s, err_t;
	err_init(&err);
	printf("\n============================================================\n");
	printf("Fixed Radix Cpp library test program\n");
	printf("M. A. Chatterjee (c) 2001-2012\n\n");
	printf("These routines were developed for use on ink recognizers, and embedded projects\n");
	printf("This sample program uses the C std floating point library to show floating / fixed point operations\n");
	printf("On embedded systems floating point won't be available so the radix scaling factors must be used.\n");

	// Fixed radix math tests
	// Simple overflow example demo, see FR_math_docs.txt for discussion, this is not part of the library
	char ca = 34, cb = 3, cc;
	printf("Overflow example using 8 bit numbers\n");
	cc = ca * cb;
	printf("%d * %d = %d\n", (int)ca, (int)cb, (int)cc);
	cb = 5;
	cc = ca * cb;
	printf("%d * %d = %d (!!)\n\n", (int)ca, (int)cb, (int)cc);

	// Fixed radix integer math test
	s32 a, b, c, s;
	printf("Fixed Radix Integer Routines\n");
	printf("\n");
	a = 1234;
	printf("Some different interpretations for %d, a signed number, based on different base-2 radixes\n", (int)a);
	for (i = 0; i < 15; i++)
	{
		printf("radix:%2d ==>", i);
		printNumSigned(a, i);
		printf(" precision = (1/%5d) or %f", (int)1 << i, FR2D(1, i));
		printf("\n");
	}
	printf("\nExamples: Adding two numbers together (signed)\n");
	for (i = 0; i < 5; i++)
	{
		u16 r = 5;
		a = i * 55;
		b = 654321;
		c = a + b;
		s = FR_FixAddSat(a, b);
		printNumSigned3(a, r, b, r, c, r);
		printf("saturated : ");
		printNumSigned(s, r);
		printf("\n");
		double af, bf, cf;
		af = FR2D(a, r);
		bf = FR2D(b, r);
		cf = af + bf;

		printf("using doubles:            af:%10.4f           bf:%10.4f           cf:%10.4f \n", af, bf, cf);
		printf("     fixed to double result delta(c_FR - cf)=%10.4f\n", FR2D(c, r) - cf);
		printf("sat  fixed to double result delta(s_FR - cf)=%10.4f\n", FR2D(s, r) - cf);
	}

	printf("\nExamples: Multiplying two numbers together (signed)\n");
	printf("Watch where overflow starts errors.  Before this fixed radix multiplies work quite well!\n");
	printf("Also look at how saturation prevents wrap-around at the expense of clipping error\n");
	for (i = 1; i < 3; i++)
	{
		u16 r = 7;
		a = i * 313;
		b = 654321;
		c = a * b;							   // remember the prec goes radix_a + radix_b
		printNumSigned3(a, r, b, r, c, 2 * r); // see notes in FR_math_docs for more on multiplicative precision
		s = FR_FixMulSat(a, b);
		printf("saturated : ");
		printNumSigned(s, r);
		printf("\n");

		double af, bf, cf;
		af = FR2D(a, r);
		bf = FR2D(b, r);
		cf = af * bf;
		printf("using doubles:            af:%10.4f           bf:%10.4f           cf:%10.4f \n", af, bf, cf);
		printf("     fixed to double result delta(c_FR - cf)=%10.4f\n", FR2D(c, 2 * r) - cf);
		printf("sat  fixed to double result delta(s_FR - cf)=%10.4f\n", FR2D(s, 2 * r) - cf);
	}

	// begin 2D Coordinate point matrix tests
	FR_Matrix2D_CPT mA, mB;
	s16 x16, y16;
	s32 x32, y32;

	printf("\nFixed Radix 2D coordinate matrix routines\n");
	printf("Create simple transformation & rotation matrix we will call mA and its inverse mB\n");
	x32 = 100;
	y32 = 200;
	mA.XlateI(15, 24);
	mB = mA;  // set B to A (note this uses C++ operator overloading
	mB.inv(); // Make B the inverse of A

	printf("Forward Transform Matrix (fixed-%dbit-radix and floating point equivalent\n", mA.radix);
	printMatrixBoth(&mA);
	printf("\n");
	printf("Reverse Transform Matrix (fixed-%dbit-radix and floating point equivalent\n", mB.radix);
	printMatrixBoth(&mB);

	printf("32bit:(%6d,%6d)\n", (int)x32, (int)y32);
	printf("Forward tranform 32 bit (x32,y32)*[mA]\n");
	mA.XFormPtI(x32, y32, &x32, &y32);

	printf("32bit:(%6d,%6d)\n", (int)x32, (int)y32);
	printf("Reverse tranform 32 bit (x32,y32)*[mB]\n");
	mB.XFormPtI(x32, y32, &x32, &y32);
	printf("32bit:(%6d,%6d)\n", (int)x32, (int)y32);

	printf("\n");
	x16 = 110;
	y16 = 210;
	printf("16bit:(%6d,%6d)\n", (int)x16, (int)y16);
	printf("Forward transform 16 bit (x16,y16)*[mA]\n");
	mA.XFormPtI16(x16, y16, &x16, &y16);

	printf("16bit:(%6d,%6d)\n", (int)x16, (int)y16);
	printf("Reverse transform 16 bit (x16,y16)*[mB]\n");
	mB.XFormPtI16(x16, y16, &x16, &y16);
	printf("16bit:(%6d,%6d)\n", (int)x16, (int)y16);

	printf("\nNow that we have translated back and forth a few points we'll do a skew/rotate\n");
	printf("to save space I'll only show this with 32 bit points but 16 bit points work too.\n");

	// add some scale/skew rotation by hand

	mA.radix = 6; // we'll use 6 bits of fractional precision or 1/32 per unit
	printf("To show the precisions effects of a lower radix we'll set the radix of matrix to %d bits\n\n", mA.radix);

	mA.ID();				  // create matrix from scratch with new precision
	mA.XlateI(15, 24);		  // add some translation
	mA.m01 = (3) << mA.radix; // that's integer 3 scaled by the fractional bits, we can also do fractional values
	mA.m10 = (2) << mA.radix; // if we shift by less than the radix
	mA.checkfast();			  // clean up after hand-setting member vars
	mA.inv(&mB);			  // set B to inverse of A
	printf("Forward Transform Matrix (fixed-%dbit-radix and floating point equivalent\n", mA.radix);
	printMatrixBoth(&mA);
	printf("\n");
	printf("Reverse Transform Matrix (fixed-%dbit-radix and floating point equivalent\n", mB.radix);
	printMatrixBoth(&mB);

	printf("32bit:(%6d,%6d)\n", (int)x32, (int)y32);
	printf("Forward tranform 32 bit (x32,y32)*[mA]=(x32new,y32new)\n");
	mA.XFormPtI(x32, y32, &x32, &y32);

	printf("32bit:(%6d,%6d)\n", (int)x32, (int)y32);
	printf("Reverse tranform 32 bit (x32new,y32new)*[mB]=*should be* (x32,y32)\n");
	mB.XFormPtI(x32, y32, &x32, &y32);
	printf("32bit:(%6d,%6d)\n", (int)x32, (int)y32);

	printf("... well look its *almost* the same but not quite! .. what is happening is we don't have enough\n");
	printf("fractional resolution.  Now lets up the radix and see what happens.\n\n");
	//==========================================

	mA.radix = 11; // we'll use 11 bits of fractional precision or 1/2048 per unit
	printf("To show how precisions effects the radix we'll set the radix of matrix to %d bits\n\n", mA.radix);
	mA.ID();				  // create matrix from scratch with new precision
	mA.XlateI(15, 24);		  // add some translation
	mA.m01 = (3) << mA.radix; // that's 3 scaled by the fractional bits
	mA.m10 = (2) << mA.radix; // same idea here
	mA.checkfast();			  // clean up after hand-setting member vars
	mA.inv(&mB);			  // set B to inverse of A
	printf("Forward Transform Matrix (fixed-%dbit-radix and floating point equivalent)\n", mA.radix);
	printMatrixBoth(&mA);
	printf("\n");
	printf("Reverse Transform Matrix (fixed-%dbit-radix and floating point equivalent)\n", mB.radix);
	printMatrixBoth(&mB);

	printf("32bit:(%6d,%6d)\n", (int)x32, (int)y32);
	printf("Forward tranform 32 bit (x32,y32)*[mA]=(x32new,y32new)\n");
	mA.XFormPtI(x32, y32, &x32, &y32);

	printf("32bit:(%6d,%6d)\n", (int)x32, (int)y32);
	printf("Reverse tranform 32 bit (x32new,y32new)*[mB]=*should be* (x32,y32)\n");
	mB.XFormPtI(x32, y32, &x32, &y32);
	printf("32bit:(%6d,%6d)\n", (int)x32, (int)y32);
	//======================================
	// rather than setting the matrix components by hand we can use  setrot
	// if you need affine / skew xforms then you'll still need to hand-mod the
	// the matrix components

	mA.radix = 8; //  bits of fractional precision or 1/(2^radix) per unit
	printf("Lets do a rotation with the radix of matrix to %d bits\n\n", mA.radix);
	mA.ID();		   // create matrix from scratch with new precision
	mA.XlateI(15, 24); // add some translation
	mA.setrotate(23);  // add some rotation

	mA.inv(&mB); // set B to inverse of A
	printf("Forward Transform Matrix (fixed-%dbit-radix and floating point equivalent)\n", mA.radix);
	printMatrixBoth(&mA);
	printf("\n");
	printf("Reverse Transform Matrix (fixed-%dbit-radix and floating point equivalent)\n", mB.radix);
	printMatrixBoth(&mB);

	printf("32bit:(%6d,%6d)\n", (int)x32, (int)y32);
	printf("Forward tranform 32 bit (x32,y32)*[mA]=(x32new,y32new)\n");
	mA.XFormPtI(x32, y32, &x32, &y32);

	printf("32bit:(%6d,%6d)\n", (int)x32, (int)y32);
	printf("Reverse tranform 32 bit (x32new,y32new)*[mB]=*should be* (x32,y32)\n");
	mB.XFormPtI(x32, y32, &x32, &y32);
	printf("32bit:(%6d,%6d)\n", (int)x32, (int)y32);

	// we're done!
	printf("\n well that's it, now remember we can also change the radix on-the-fly by changing\n");
	printf("the radix member variable in the matrix.  Just watch your prescision!\n");
	printf("for more info see the accompanying FR_math_docs.txt\n");
	// Degree based trig tests
	if (gTestForwardTrig)
	{

		index = 0;
		err_init(&err_c);
		err_init(&err_s);
		err_init(&err_t);
		for (i = -370; i < 370; i += 1)
		// for (i=0; i< 91; i+= 1)
		{
			double fc, zc = cos(((double)i) * (R2D));
			double fs, zs = sin(((double)i) * (R2D));
			double ft, zt;
			if ((zc > 0.001) || (zc < (-0.001)))
				zt = tan(((double)i) * (R2D));
			else
				zt = zs > 0.0 ? 32767 : -32767;

			fc = FR2D(FR_CosI(i), 15);
			fs = FR2D(FR_SinI(i), 15);
			ft = FR2D(FR_TanI(i), 15);
			err_accum(&err_c, fc, zc);
			err_accum(&err_s, fs, zs);
			err_accum(&err_t, ft, zt);
			printf("%4ld : %4d : %9.5f %9.5f %9.5f : %9.5f %9.5f %9.5f : %9.5f %9.5f %9.5f\n",
				   index, i, fc, zc, FR_ERR(fc, zc), fs, zs, FR_ERR(fs, zs), ft, zt, FR_ERR(ft, zt));
			index++;
		}
		err_print(&err_c, "cos:");
		err_print(&err_s, "sin:");
		err_print(&err_t, "tan:");
	}
	// Radian based trig testing
	if (gTestRadians)
	{
		printf("begin radian based trig\n");
		index = 0;
		err_init(&err_c);
		err_init(&err_s);
		err_init(&err_t);
		{
			double lcl2pi = 2.0 * 3.14159265358;
			double id;
			;
			for (id = -lcl2pi; id <= lcl2pi; id += 0.01)
			{
				double fc, zc = cos(id);
				double fs = 0, zs = sin(id);
				double ft = 0, zt;

				if ((zc > 0.001) || (zc < (-0.001)))
					zt = tan(id);
				else
					zt = zs > 0.0 ? 32767 : -32767;
				s32 v = (s32)(id * 32768.0);
				fc = FR2D(FR_cos(v, 15), 15);
				// fs = FR2D(FR_SinI(i),15);
				// ft = FR2D(FR_TanI(i),15);
				err_accum(&err_c, fc, zc); // err_accum(&err_s,fs,zs);err_accum(&err_t,ft,zt);
				printf("%4ld : %7.3f : %9.5f %9.5f %9.5f : %9.5f %9.5f %9.5f : %9.5f %9.5f %9.5f\n",
					   index, id * 360.0 / lcl2pi, fc, zc, FR_ERR(fc, zc), fs, zs, FR_ERR(fs, zs), ft, zt, FR_ERR(ft, zt));
				index++;
			}
			err_print(&err_c, "cos:");
			err_print(&err_s, "sin:");
			err_print(&err_t, "tan:");
		}
	}

	if (gTestRadianMACROS)
	{
		// rad to deg, deg to radian macro test
		for (i = -233; i <= 230; i += 7)
		{
			double deg, rad, df, rf;
			s32 ir, r = 16;

			ir = I2FR(i, r);
			rf = (double)i * (0.017453292519943295769236);
			df = (double)i * (57.29577951308232087679815);
			deg = FR2D(FR_DEG2RAD(ir), r);
			rad = FR2D(FR_RAD2DEG(ir), r);
			printf("%4d %12.5f %12.5f %12.5f %12.5f %12.5f\n", i, (double)ir, df, deg, rf, rad);
		}
	}
	if (gTestInvTrig)
	{
		for (i = -32768; i <= 32768; i += 1 << 6)
		{
			double fi = (double)i / (32768.0);
			double fac = acos(fi) * 57.29577951308232087679815;
			double fr_ac = FR2D(FR_acos(i, FR_TRIG_PREC), 0);
			printf("%6d : %8.4f %8.4f %8.4f %8.4f \n", i, fi, fac, fr_ac, FR_ERR(fac, fr_ac));
		}
	}
	{
		// quandrant = FR_DEG2Q(FR_CHRDX(x,xr,0))&3;
		// quandrant_pos = (FR_DEG2Q(FR_CHRDX(x,xr,0))&3)+4)&3);
	}
	if (gTestPow2andLog2)
	{
		printf("begin power2, log2\n");
		index = 0;
		FR_TEST_ERR err_p2, err_pe, err_p10;
		err_init(&err_p2);
		err_init(&err_pe);
		err_init(&err_p10);
		{
			double id;
			// test power 2
			for (id = -3.0; id <= 3.0; id += (1.0 / 32.0))
			{
				double fp2, fpe, fp10, zp2 = pow(2, id);
				double zpe = pow(2.71828182845, id);
				double zp10 = pow(10, id);
				s32 v = (s32)(id * 32768);
				fp2 = FR2D(FR_pow2(v, 15), 15);
				fpe = FR2D(FR_EXP(v, 15), 15);
				fp10 = FR2D(FR_POW10(v, 15), 15);
				err_accum(&err_p2, fp2, zp2);
				err_accum(&err_pe, fpe, zpe);
				err_accum(&err_p10, fp10, zp10);
				printf("%4ld: %7.3f : %11.5f %11.5f %11.5f : %11.5f %11.5f %11.5f : %11.5f %11.5f %11.5f\n", index, id,
					   fp2, zp2, FR_ERR(fp2, zp2), fpe, zpe, FR_ERR(fpe, zpe), fp10, zp10, FR_ERR(fp10, zp10));
				index++;
			}
			err_print(&err_p2, "pow2 :");
			err_print(&err_pe, "exp  :");
			err_print(&err_p10, "pow10:");

			/*
			FR_TEST_ERR err_l2, err_le, err_l10;
			//test log2, ln, log10
			err_init(&err_l2);
			for (id = 0.1; id<= 25.0; id+= 0.1)
			{
				double fl2  ,zl2=log(id)/log(2);

				s32 v = (s32)(id*32768.0);
				fl2   = FR2D(FR_log2(v,15,15),15);

				err_accum(&err_l2,fl2,zl2);
				printf("%4d : %7.3f : %9.5f %9.5f %9.5f\n",	index,id,fl2,zl2,FR_ERR(fl2,zl2));
				index++;

			}
			err_print(&err_p2,"pow2:");
			*/
		}
	}

	printf("\nTes FR_printNum(..) fammily of functions showing various prec choices\n");
	{
		int rdx = 13;
		int z = (int)((123.45678) * (1 << 13));
		int zn = -z;
		printf("z (int) %8d,  zn (int) %8d\n", z, zn);
		printf("z %9.3f    zn %9.3f\n", FR2D(z, rdx), FR2D(zn, rdx));

		printf("z  using printNumF( <serialOut> , z,%3d,4,3) :  ", rdx);
		FR_printNumF(putSingleChar, z, rdx, 4, 3);
		printf("\n");
		printf("zn using printNumF( <serialOut> ,zn,%3d,4,3) : ", rdx);
		FR_printNumF(putSingleChar, zn, rdx, 4, 3);
		printf("\n");

		printf("z  using printNumF( <serialOut> , z,%3d,5,2) :", rdx);
		FR_printNumF(putSingleChar, z, rdx, 5, 2);
		printf("\n");
		printf("zn using printNumF( <serialOut> ,zn,%3d,5,2) :", rdx);
		FR_printNumF(putSingleChar, zn, rdx, 5, 2);
		printf("\n");

		printf("z  using printNumF( <serialOut> , z,%3d,5,5) :", rdx);
		FR_printNumF(putSingleChar, z, rdx, 5, 5);
		printf("\n");
		printf("zn using printNumF( <serialOut> ,zn,%3d,5,5) :", rdx);
		FR_printNumF(putSingleChar, zn, rdx, 5, 5);
		printf("\n");

		printf(" check floor and ceil macros\n");
		printf("ceil:    z,  zn   (%10.5f,%10.5f) FR:", FR2D(FR_CEIL(z, rdx), rdx), FR2D(FR_CEIL(zn, rdx), rdx));
		FR_printNumF(putSingleChar, FR_CEIL(z, rdx), rdx, 5, 5);
		printf(" , ");
		FR_printNumF(putSingleChar, FR_CEIL(zn, rdx), rdx, 5, 5);
		printf("\n\n");

		printf("floor:   z,  zn   (%10.5f,%10.5f) FR:", FR2D(FR_FLOOR(z, rdx), rdx), FR2D(FR_FLOOR(zn, rdx), rdx));
		FR_printNumF(putSingleChar, FR_FLOOR(z, rdx), rdx, 5, 5);
		printf(" , ");
		FR_printNumF(putSingleChar, FR_FLOOR(zn, rdx), rdx, 5, 5);
		printf("\n\n");
	}

	return ret_val;
}
