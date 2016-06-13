/**
 *
 *	@file FR_mathroutines.cpp - c++ implementation file for fixed radix math routines
 *	
 *	@copy Copyright (C) <2001-2014>  <M. A. Chatterjee>
 *  @author M A Chatterjee <deftio [at] deftio [dot] com>
 *	@version 1.01 M. A. Chatterjee, cleaned up naming
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
 

#include "FR_mathroutines.h"


//=======================================================
// FR_FixMuls (x*y signed NOT saturated mul)
// User must keep track of new radix point after using this function
//

s32 FR_FixMuls(s32 x, s32 y) // signed not saturated
{
	s32 z,sign = (x < 0)? (y>0) : (y < 0);

	x = FR_ABS(x);
	y = FR_ABS(y);
	z = FR_FIXMUL32u(x,y);
	
	return sign?-z:z;	
}

//=======================================================
// FR_FixMulSat (x*y saturated multiply)
// User must keep track of new radix point after using this function
//

s32 FR_FixMulSat(s32 x, s32 y)
{
	s32 z,h,l,m1,m2,sat = 0;
	int sign = ( ((x<0)&& (y>0)) || ((x>0)&&(y<0)) ) ?1:0;
	
	x = FR_ABS(x);
	y = FR_ABS(y);
	h  = ((x>>16)*(y>>16));	
	m1 = ((x>>16)*(y&0xffff));
	m2 = ((y>>16)*(x&0xffff));
	l  = (((x&0xffff)*(y&0xffff)));
	
	z = (h<<16)+m1+m2+l;

	if (h&0xffff8000)  // TODO: make better saturation check -- this might be a bug
		z = 0x7fffffff;
	else
	{
		z = (h<<16) + m1;
		if (z < 0)
			z = 0x7fffffff;
		else
		{
			z+= m2;
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
	return sign?-z:z;	
}

/*=======================================================
  FR_FixAddSat (x+y saturated add)
  programmer must align radix points before using this function
 */
s32 FR_FixAddSat(s32 x, s32 y)
{
	s32 sum = x+y;
	if (x < 0)
	{
		if (y < 0)
			return (sum >= 0)?0x80000000:sum;
	}
	else
	{
		if (y >= 0)
			return (sum <= 0)?0x7fffffff:sum;	
	}
	return sum;
}


/* Cosine table in s0.15 format, 1 entry per degree
 * used in all trig functions (sin,cos,tan, atan2 etc)
 */
s16 const static gFR_COS_TAB_S0d15[90]={
    32767,32762,32747,32722,32687,32642,32587,32522,32448,32363,
    32269,32164,32050,31927,31793,31650,31497,31335,31163,30981,
    30790,30590,30381,30162,29934,29696,29450,29195,28931,28658,
    28377,28086,27787,27480,27165,26841,26509,26168,25820,25464,
    25100,24729,24350,23964,23570,23169,22761,22347,21925,21497,
    21062,20620,20173,19719,19259,18794,18323,17846,17363,16876,
    16383,15885,15383,14875,14364,13847,13327,12803,12274,11742,
    11207,10668,10125, 9580, 9032, 8481, 7927, 7371, 6813, 6252,
     5690, 5126, 4560, 3993, 3425, 2856, 2286, 1715, 1144,  572,
        0};
    
/* cosine with integer input precision in degrees, returns s0.15 result
 */
s16 FR_CosI(s16 deg) 
{
	deg = ((deg%360)+360)%360; /* this is an expensive operation*/
	if (deg<180)
		return   deg<90 ? gFR_COS_TAB_S0d15[deg    ]:-gFR_COS_TAB_S0d15[180-deg];		
	else
		return -(deg<270? gFR_COS_TAB_S0d15[deg-180]:-gFR_COS_TAB_S0d15[360-deg]);	
}

/* sin with integer input precision in degrees, returns s0.15 result
 */
s16 FR_SinI(s16 deg)
{
	return FR_CosI(deg-90);
}

/* cos() with fixed radix precision, returns interpolated s0.15 result
 */
s16 FR_Cos (s16 deg,u16 radix)
{
	s16 i,j;
	i = FR_CosI(deg>>radix);
	j=  FR_CosI((deg>>radix)+1);
	return i+(((j-i)*(deg&((1<<radix)-1)))>>radix);
}

/* sin() with fixed radix precision,  returns interpolated s0.15 result
 * could be a macro..
 */
s16 FR_Sin (s16 deg,u16 radix)
{
	return FR_Cos(deg-(90<<radix),radix);
}

/* tan with s15.16 result 
 * could also have tableized (like Cos implementation)
 * note: tan(90)  returns 32727 
 * and   tan(270) returns (-32727) (e.g. no div by zero)
 */
s32 FR_TanI (s16 deg)
{
	s32 c = FR_CosI(deg);
	s32 s = FR_SinI(deg);
	return (c!=0)?(s<<FR_TRIG_PREC)/c : (s>0)?FR_TRIG_MAXVAL:FR_TRIG_MINVAL;
}

/* Tan with s15.16 result with fixed radix input precision, returns interpolated s15.16 result
 */
 
s32 FR_Tan (s16 deg, u16 radix)
{
	s16 i,j;
	i = FR_TanI(deg>>radix);
	j=  FR_TanI((deg>>radix)+1);
	return i+(((j-i)*(deg&((1<<radix)-1)))>>radix);
}

//Inverse Trig
//Ugly looking acos with bin search (working):
s16 FR_acos (s32 input, u16 radix)
{
	s16 r=45, s=input, x=46,y,z;	
	
	input = FR_CHRDX(input,radix,FR_TRIG_PREC); /* chg radix to s0.15 */
	
	// +or- 1.0000 is special case as it doesn't fit in table search 
	if ((input&0xffff)==0x8000) //? shouldn't it be: (input&7fff)!=0 
		return (input<0) ? 180: 0;
	input = (FR_ABS(input)) & ((1<<radix)-1);	
	while (x>>=1)
		r+= (input < gFR_COS_TAB_S0d15[r]) ? x : -x;
	
	r+= (input <  gFR_COS_TAB_S0d15[r]) ? 1 : -1;
	r+= (input <  gFR_COS_TAB_S0d15[r]) ? 1 : -1;
	
	x = FR_ABS(input - gFR_COS_TAB_S0d15[r]);
	y = FR_ABS(input - gFR_COS_TAB_S0d15[r+1]);
	z = FR_ABS(input - gFR_COS_TAB_S0d15[r-1]);
	r = (x<y) ? r: r+1;
	r = (x<z) ? r: r-1;

	return (s>0) ? r:180-r;;	 
}



s16 FR_asin (s32 input, u16 radix)
{
	return 90-FR_acos(input,radix);	 
}

//=======================================================
// Matrix Functions


void FR_Matrix2D_CPT::ID()
{
	m00 = I2FR(1,radix);
	m01 = 0;
	m02 = 0;

	m10 = 0;
	m11 = I2FR(1,radix);
	m12 = 0;

	fast = checkfast();
}

// det() computes the determinant based on the assumption that these are 
// coordinate x-form matrices and  not general purpose 3x3 matrices 
s32 FR_Matrix2D_CPT ::det ()
{
	// det = m00 * m11 - m01 * m10
	return FR_FixAddSat(FR_FixMulSat(m00,m11), -FR_FixMulSat(m01,m10))>>radix;
}

// inv() computes inverse of a coordinate transform matrix
// actual representation is as below:
// m00	m01	m02
// m10	m11	m12
// 0	0	1
// note there are 6 divides here -- this is not a performance issue as these only occur
// when once each time the user changes the zoom or rotation etc.  This fn is not called
// in the rendering loop

FR_RESULT FR_Matrix2D_CPT ::inv ( FR_Matrix2D_CPT *npI)
{
	s32 d = det();
	if (0 == d)
		return FR_E_UNABLE;

	if (npI == this)
	{
		return FR_E_BADARGUMENTS;
	}
	npI->m00 = (  m11 <<radix) / d;		
	npI->m01 = ((-m01)<<radix) / d;
	npI->m02 = (FR_FixAddSat( FR_FixMulSat(m01,m12), -FR_FixMulSat(m02,m11) ))/d;

	npI->m10 = ((-m10)<<radix) / d;
	npI->m11 = (  m00 <<radix) / d;
	npI->m12 = (FR_FixAddSat( FR_FixMulSat(m02,m10), -FR_FixMulSat(m00,m12) ))/d;

	npI->fast = npI->checkfast();
	npI->radix= radix;
		 
	return FR_S_OK;
}
//===============================================================
// compute inverse of self, note some matrices not invertable
FR_RESULT FR_Matrix2D_CPT ::inv ()
{
	FR_RESULT res = FR_S_OK;
	FR_Matrix2D_CPT n;
	inv(&n);
	*this=n;
	return res;
}
//================================================================
//set up an integer rotation in degrees.
FR_RESULT    FR_Matrix2D_CPT ::setrotate (s16 deg)
{
	m00 =  (FR_CosI(deg))>>(FR_TRIG_PREC-radix); // the 15 comes from Cos/Sin precision
	m01 = -(FR_SinI(deg))>>(FR_TRIG_PREC-radix); // see those fns for more details
	m10 =  (FR_SinI(deg))>>(FR_TRIG_PREC-radix);
	m11 =  (FR_CosI(deg))>>(FR_TRIG_PREC-radix);
	checkfast();
}
//================================================================
//set up a rotation with fixed radix input precision
FR_RESULT    FR_Matrix2D_CPT ::setrotate (s16 deg, u16 deg_radix)	   
{
	m00 =  (FR_Cos(deg,deg_radix))>>(FR_TRIG_PREC-radix); // the 15 comes from Cos/Sin precision
	m01 = -(FR_Sin(deg,deg_radix))>>(FR_TRIG_PREC-radix); // see those fns for more details
	m10 =  (FR_Sin(deg,deg_radix))>>(FR_TRIG_PREC-radix);
	m11 =  (FR_Cos(deg,deg_radix))>>(FR_TRIG_PREC-radix);
	checkfast();
}
//================================================================
// Add two matrices together

FR_RESULT FR_Matrix2D_CPT ::add(const FR_Matrix2D_CPT *pAdd)
{
	m00 = FR_FixAddSat (m00,pAdd->m00);
	m01 = FR_FixAddSat (m01,pAdd->m01);
	m02 = FR_FixAddSat (m02,pAdd->m02);
	m10 = FR_FixAddSat (m10,pAdd->m10);
	m11 = FR_FixAddSat (m11,pAdd->m11);
	m12 = FR_FixAddSat (m12,pAdd->m12);
	checkfast();
	return FR_S_OK;
}
//=======================================================
// Sub two matrices this -= pSub

FR_RESULT FR_Matrix2D_CPT ::sub(const FR_Matrix2D_CPT *pSub)
{
	m00 = FR_FixAddSat (m00,-(pSub->m00));
	m01 = FR_FixAddSat (m01,-(pSub->m01));
	m02 = FR_FixAddSat (m02,-(pSub->m02));
	m10 = FR_FixAddSat (m10,-(pSub->m10));
	m11 = FR_FixAddSat (m11,-(pSub->m11));
	m12 = FR_FixAddSat (m12,-(pSub->m12));
	checkfast();
	return FR_S_OK;
}

//=======================================================
// set a matrix with the provided data

void FR_Matrix2D_CPT ::set(s32 a00, s32 a01, s32 a02, s32 a10, s32 a11, s32 a12, u16 nRadix )
{
	m00 = a00;
	m01 = a01;
	m02 = a02;
	m10 = a10;
	m11 = a11;
	m12 = a12; 
	radix = nRadix;
	checkfast();
}
//=======================================================
// standard matrix operators

FR_Matrix2D_CPT& FR_Matrix2D_CPT ::operator += (const FR_Matrix2D_CPT &nM)
{
	add(&nM);
	return *this;
}

FR_Matrix2D_CPT& FR_Matrix2D_CPT ::operator -= (const FR_Matrix2D_CPT &nM)
{
	sub(&nM);
	return *this;
}

FR_Matrix2D_CPT& FR_Matrix2D_CPT ::operator *= (const s32 &X)
{
	m00 = FR_FixMulSat(m00,X);
	m01 = FR_FixMulSat(m01,X);
	m02 = FR_FixMulSat(m02,X);
	m10 = FR_FixMulSat(m10,X);
	m11 = FR_FixMulSat(m11,X);
	m12 = FR_FixMulSat(m12,X);
	checkfast();
	return *this;
}

FR_Matrix2D_CPT& FR_Matrix2D_CPT ::operator = (const FR_Matrix2D_CPT &nM)
{
	m00 = nM.m00;
	m01 = nM.m01;
	m02 = nM.m02;
	m10 = nM.m10;
	m11 = nM.m11;
	m12 = nM.m12; 
	radix = nM.radix;
	checkfast();
	return *this;
}

