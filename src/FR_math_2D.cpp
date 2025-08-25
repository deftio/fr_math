/**
 *
 *	@file FR_math_2D.cpp - c++ implementation file for fixed radix math
 *                       - 2D simple matrices
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

#include "FR_math_2D.h"
#include "FR_math.h"
// namespace FR_MATH {
//=======================================================
//  Matrix Functions

void FR_Matrix2D_CPT::ID()
{
	m00 = I2FR(1, radix);
	m01 = 0;
	m02 = 0;

	m10 = 0;
	m11 = I2FR(1, radix);
	m12 = 0;

	fast = checkfast();
}

// det() computes the determinant based on the assumption that these are
// coordinate x-form matrices and  not general purpose 3x3 matrices
s32 FR_Matrix2D_CPT ::det()
{
	// det = m00 * m11 - m01 * m10
	return FR_FixAddSat(FR_FixMulSat(m00, m11), -FR_FixMulSat(m01, m10)) >> radix;
}

// inv() computes inverse of a coordinate transform matrix
// actual representation is as below:
// m00	m01	m02
// m10	m11	m12
// 0	0	1
// note there are 6 divides here -- this is not a performance issue as these only occur
// when once each time the user changes the zoom or rotation etc.  This fn is not called
// in the rendering loop

FR_RESULT FR_Matrix2D_CPT ::inv(FR_Matrix2D_CPT *npI)
{
	s32 d = det();
	if (0 == d)
		return FR_E_UNABLE;

	if (npI == this)
	{
		return FR_E_BADARGUMENTS;
	}
	npI->m00 = (m11 << radix) / d;
	npI->m01 = ((-m01) << radix) / d;
	npI->m02 = (FR_FixAddSat(FR_FixMulSat(m01, m12), -FR_FixMulSat(m02, m11))) / d;

	npI->m10 = ((-m10) << radix) / d;
	npI->m11 = (m00 << radix) / d;
	npI->m12 = (FR_FixAddSat(FR_FixMulSat(m02, m10), -FR_FixMulSat(m00, m12))) / d;

	npI->fast = npI->checkfast();
	npI->radix = radix;

	return FR_S_OK;
}
//===============================================================
// compute inverse of self, note some matrices not invertable
FR_RESULT FR_Matrix2D_CPT ::inv()
{
	FR_RESULT res = FR_S_OK;
	FR_Matrix2D_CPT n;
	inv(&n);
	*this = n;
	return res;
}
//================================================================
// set up an integer rotation in degrees.
FR_RESULT FR_Matrix2D_CPT ::setrotate(s16 deg)
{
	m00 = (FR_CosI(deg)) >> (FR_TRIG_PREC - radix);	 // the 15 comes from Cos/Sin precision
	m01 = -(FR_SinI(deg)) >> (FR_TRIG_PREC - radix); // see those fns for more details
	m10 = (FR_SinI(deg)) >> (FR_TRIG_PREC - radix);
	m11 = (FR_CosI(deg)) >> (FR_TRIG_PREC - radix);
	checkfast();
	return FR_S_OK;
	;
}
//================================================================
// set up a rotation with fixed radix input precision
FR_RESULT FR_Matrix2D_CPT ::setrotate(s16 deg, u16 deg_radix)
{
	m00 = (FR_Cos(deg, deg_radix)) >> (FR_TRIG_PREC - radix);  // the 15 comes from Cos/Sin precision
	m01 = -(FR_Sin(deg, deg_radix)) >> (FR_TRIG_PREC - radix); // see those fns for more details
	m10 = (FR_Sin(deg, deg_radix)) >> (FR_TRIG_PREC - radix);
	m11 = (FR_Cos(deg, deg_radix)) >> (FR_TRIG_PREC - radix);
	checkfast();
	return FR_S_OK;
}
//================================================================
// Add two matrices together

FR_RESULT FR_Matrix2D_CPT ::add(const FR_Matrix2D_CPT *pAdd)
{
	m00 = FR_FixAddSat(m00, pAdd->m00);
	m01 = FR_FixAddSat(m01, pAdd->m01);
	m02 = FR_FixAddSat(m02, pAdd->m02);
	m10 = FR_FixAddSat(m10, pAdd->m10);
	m11 = FR_FixAddSat(m11, pAdd->m11);
	m12 = FR_FixAddSat(m12, pAdd->m12);
	checkfast();
	return FR_S_OK;
}
//=======================================================
// Sub two matrices this -= pSub

FR_RESULT FR_Matrix2D_CPT ::sub(const FR_Matrix2D_CPT *pSub)
{
	m00 = FR_FixAddSat(m00, -(pSub->m00));
	m01 = FR_FixAddSat(m01, -(pSub->m01));
	m02 = FR_FixAddSat(m02, -(pSub->m02));
	m10 = FR_FixAddSat(m10, -(pSub->m10));
	m11 = FR_FixAddSat(m11, -(pSub->m11));
	m12 = FR_FixAddSat(m12, -(pSub->m12));
	checkfast();
	return FR_S_OK;
}

//=======================================================
// set a matrix with the provided data

void FR_Matrix2D_CPT ::set(s32 a00, s32 a01, s32 a02, s32 a10, s32 a11, s32 a12, u16 nRadix)
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

FR_Matrix2D_CPT &FR_Matrix2D_CPT ::operator+=(const FR_Matrix2D_CPT &nM)
{
	add(&nM);
	return *this;
}

FR_Matrix2D_CPT &FR_Matrix2D_CPT ::operator-=(const FR_Matrix2D_CPT &nM)
{
	sub(&nM);
	return *this;
}

FR_Matrix2D_CPT &FR_Matrix2D_CPT ::operator*=(const s32 &X)
{
	m00 = FR_FixMulSat(m00, X);
	m01 = FR_FixMulSat(m01, X);
	m02 = FR_FixMulSat(m02, X);
	m10 = FR_FixMulSat(m10, X);
	m11 = FR_FixMulSat(m11, X);
	m12 = FR_FixMulSat(m12, X);
	checkfast();
	return *this;
}

FR_Matrix2D_CPT &FR_Matrix2D_CPT ::operator=(const FR_Matrix2D_CPT &nM)
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
//} //end namespace FR_MATH
