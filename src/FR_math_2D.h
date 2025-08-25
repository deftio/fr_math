/**
 *	@FR_mathroutines.h - header definition file for fixed radix 2D coordinate transforms
 *
 *	@copy Copyright (C) <2001-2012>  <M. A. Chatterjee>
 *  @author M A Chatterjee <deftio [at] deftio [dot] com>
 *	@version 1.0.3 M. A. Chatterjee, cleaned up naming
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

#ifndef __FR_math_2D_h__
#define __FR_math_2D_h__

#ifdef __cplusplus
// namespace  FR_MATH {
extern "C"
{
#endif

#ifndef __FR_Platform_Defs_H__
#include "FR_defs.h"
#endif

#ifndef __FR_Math_h__
#include "FR_math.h"
#endif

//===============================================
// 2D Fixed radix matrix class for Coordinate Point Transforms (CPT)
// This is *NOT* a complete matrix class just something for common 2D point transformations.
// some operators not provided because the programmer needs to make decisions about
// precision/radix tradeoffs
//================================================
// Simple class to handle coord transforms
#define FR_MAT_DEFPREC (8) // default precision radix

	struct FR_Matrix2D_CPT
	{
		// note: if modifying these variables by hand be sure to call checkfast() afterwards
		s32 m00;
		s32 m01;
		s32 m02;
		s32 m10;
		s32 m11;
		s32 m12;

		// don't need m20 .. m22 because always 0 0 1 in coord-matrices
		// wrote out m00 to make code easier to read in fix point (see the .cpp)

		u16 radix; // internal precision radix point
		int fast;  // for taking advantage of scale-only matrices

		//========================
		void ID(); // create Identity matrix

		//=======================
		FR_Matrix2D_CPT(u16 nRadix = FR_MAT_DEFPREC) : radix(nRadix) { ID(); }; // constructor

		// matrix operators
		s32 det();									// compute determinant, result is in same precision as radix (not 2*radix)
		FR_RESULT inv(FR_Matrix2D_CPT *nInv);		// compute matrix inverse and put in nInv
		FR_RESULT inv();							// compute matrix inverse of this
		FR_RESULT add(const FR_Matrix2D_CPT *pAdd); // matrix this = this+pAdd;
		FR_RESULT sub(const FR_Matrix2D_CPT *pSub); // matrix this = this-pSub;
		FR_RESULT setrotate(s16 deg);				// set upr left 2x2 to rot matrix
		FR_RESULT setrotate(s16 deg, u16 radix);	// set upr left 2x2 to rot matrix

		FR_Matrix2D_CPT &operator=(const FR_Matrix2D_CPT &nM);
		FR_Matrix2D_CPT &operator+=(const FR_Matrix2D_CPT &nM);
		FR_Matrix2D_CPT &operator-=(const FR_Matrix2D_CPT &nM);

		// scalar operators
		FR_Matrix2D_CPT &operator*=(const s32 &X);

		bool checkfast()
		{
			fast = ((m01 == 0) && (m10 == 0)) ? true : false;
			return fast;
		};
		void set(s32 a00, s32 a01, s32 a02, s32 a10, s32 a11, s32 a12, u16 nRadix = FR_MAT_DEFPREC);

		// coordinate transform fns
		void XlateI(s32 x, s32 y)
		{
			m02 = x << radix;
			m12 = y << radix;
		}
		void XlateI(s32 x, s32 y, u16 nRadix)
		{
			m02 = x << nRadix;
			m12 = y << nRadix;
		}
		void XlateRelativeI(s32 x, s32 y)
		{
			m02 += x << radix;
			m12 += y << radix;
		}
		void XlateRelativeI(s32 x, s32 y, u16 nRadix)
		{
			m02 += x << nRadix;
			m12 += y << nRadix;
		}

		//========================
		// XFormPtI takes Integer input and produces fixed pt output for multiple Xforms
		// user is responsible for watching location of radix point.  For integer results use:
		// MyMatrix.XFormPtI(x,y,&xp,&yp,MyMatrix.radix);
		// note that all precision etc. has been precomputed in inv()
		// take a point and XForm it to coords represented by this matrix
		void inline XFormPtI(s32 x, s32 y, s32 *xp, s32 *yp, u16 r)
		{
			if (fast)
			{
				*xp = (x * m00 + m02) >> r;
				*yp = (y * m11 + m12) >> r;
			}
			else
			{
				*xp = (x * m00 + y * m01 + m02) >> r;
				*yp = (x * m10 + y * m11 + m12) >> r;
			}
		}
		void inline XFormPtI(s32 x, s32 y, s32 *xp, s32 *yp)
		{
			if (fast)
			{
				*xp = (x * m00 + m02) >> radix;
				*yp = (y * m11 + m12) >> radix;
			}
			else
			{
				*xp = (x * m00 + y * m01 + m02) >> radix;
				*yp = (x * m10 + y * m11 + m12) >> radix;
			}
		}
		// take a point and XForm it to coords represented by this matrix w/o translation
		void inline XFormPtINoTranslate(s32 x, s32 y, s32 *xp, s32 *yp, u16 r)
		{
			if (fast)
			{
				*xp = (x * m00) >> r;
				*yp = (y * m11) >> r;
			}
			else
			{
				*xp = (x * m00 + y * m01) >> r;
				*yp = (x * m10 + y * m11) >> r;
			}
		}

		//========================
		// XFormPtI16 takes Integer input and produces Integer output for quikr needs
		// take a point and XForm it to coords represented by this matrix
		void inline XFormPtI16(s16 x, s16 y, s16 *xp, s16 *yp)
		{
			if (fast)
			{
				*xp = (s16)((((s32)x) * m00 + m02) >> radix);
				*yp = (s16)((((s32)y) * m11 + m12) >> radix);
			}
			else
			{
				*xp = (s16)((((s32)x) * m00 + ((s32)y) * m01 + m02) >> radix);
				*yp = (s16)((((s32)x) * m10 + ((s32)y) * m11 + m12) >> radix);
			}
		}

		// take a point and XForm it to coords represented by this matrix (no translate)
		void inline XFormPtI16NoTranslate(s16 x, s16 y, s16 *xp, s16 *yp)
		{
			if (fast)
			{
				*xp = (s16)((((s32)x) * m00) >> radix);
				*yp = (s16)((((s32)y) * m11) >> radix);
			}
			else
			{
				*xp = (s16)((((s32)x) * m00 + ((s32)y) * m01) >> radix);
				*yp = (s16)((((s32)x) * m10 + ((s32)y) * m11) >> radix);
			}
		}
	};

#ifdef __cplusplus
} // extern "C"
//} // name space
#endif

#endif /* __FR_math_2D_h__ */
