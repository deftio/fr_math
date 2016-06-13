/**
 *
 *	@file FR_math.c - c implementation file for basic fixed 
 *                              radix math routines
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
 *	in a product, please place an acknowledgment in the product documentation.
 *
 *	2. Altered source versions must be plainly marked as such, and must not be
 *	misrepresented as being the original software.
 *
 *	3. This notice may not be removed or altered from any source
 *	distribution.
 *
 */
 

#include "FR_trigDegrees.h"


/*
 internally used MACROs.  renamed here to avoid name collisions with other FR_xx macros/libs if this file is 
 included in source form with other FR_math library source files.
 for external use, see FR_math.h which has equivalent exported functions/macros.
 */

#define FR_CHRDX_DEG(x,r_cur,r_new)	(((r_cur)-(r_new))>=0?((x)>>((r_cur)-(r_new))):((x)<<((r_new)-(r_cur))))


/* FR_INTERPI_DEF is the same as FR_INTERP except that  insures the
 * range is btw [0<= delta < 1] --> ((0 ... (1<<prec)-1)
 * Note: delta should be >= 0
 */
#define FR_INTERPI_DEG(x0,x1,delta,prec) ((x0)+((((x1)-(x0))*((delta)&((1<<(prec))-1)))>>(prec)))


/***********
  NEED: 
  Macros:
    FR_INTERPI, FR_ABS, FR_CHRDX
  
  constants:
    FR_TRIG_MAXVAL, FR_TRIG_MINVAL,FR_TRIG_PREC
    
 ***********/


/* Cosine table in s0.15 format, 1 entry per degree
 * used in all trig functions (sin,cos,tan, atan2 etc)
 */
s16 const static gFR_COS_TAB_S0d15[]={
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
    
/* cosine with integer input precision in degrees, returns s0.15 result */
s16 FR_cosDI(s16 deg) 
{
	deg = deg%360; /* this is an expensive operation*/
	if (deg > 180) { deg -= 360; }
	else if (deg < -180) { deg += 360;}
	
	if (deg >= 0)
		return   deg <=  (90) ?  gFR_COS_TAB_S0d15[ deg ]:-gFR_COS_TAB_S0d15[180-deg];		
	else
		return   deg >= (-90) ?  gFR_COS_TAB_S0d15[-deg ]:-gFR_COS_TAB_S0d15[180+deg];	
}
/* sin with integer input precision in degrees, returns s0.15 result    */
s16 FR_sinDI(s16 deg)
{
	return FR_sinD(deg-90);
}

/* cos() with fixed radix precision, returns interpolated s0.15 result, 
   input is in degrees 
 */
s16 FR_cosD (s16 deg,u16 radix)
{
	s16 i,j;
	i = FR_cosDegI(deg>>radix);
	j=  FR_cosDegI((deg>>radix)+1);
	return i+(((j-i)*(deg&((1<<radix)-1)))>>radix);
}

/* sin() with fixed radix precision,  returns interpolated s0.15 result
 * could be a macro..
 */
s16 FR_sinD (s16 deg,u16 radix)
{
	return FR_Cos(deg-(90<<radix),radix);
}

s16 const static gFR_TAND_TAB[]={
        1,     572,    1144,    1717,    2291,    2867,    3444,    4023,    4605,    5189,
     5777,    6369,    6964,    7564,    8169,    8779,    9395,   10017,   10646,   11282,
    11926,   12578,   13238,   13908,   14588,   15279,   15981,   16695,   17422,   18163,
    18918,   19688,   20475,   21279,   22101,   22943,   23806,   24691,   25600,   26534,
	27494,   28483,   29503,   30555,   31642,   32767};

/* tan with s15.16 result 
 * tan without table.
 * note: tan(90)  returns   32727 
 * and   tan(270) returns (-32727) (e.g. no div by zero)
 */
/*
s32 FR_tanDegI (s16 deg) 
{
	s32 c = FR_cosDegI(deg);
	s32 s = FR_sinDegI(deg);
	return (c!=0)?(s<<FR_TRIG_PREC)/c : (s>=0)?(FR_TRIG_MAXVAL<<FR_TRIG_PREC):(FR_TRIG_MINVAL<<FR_TRIG_PREC);
}
*/
#define FR_TNDEG(a) (((a)<= 45)?gFR_TAND_TAB[(a)]:(FR_TRIG_MAXVAL<<FR_TRIG_PREC)/(gFR_TAND_TAB[90-(a)]))	

s32 FR_tanDegI (s16 deg)
{
	deg = deg%360; /* this is an expensive operation*/
	if (deg > 180) { deg -= 360; }
	else if (deg < -180) { deg += 360;}

	if (90  == deg)
		return (FR_TRIG_MAXVAL<<FR_TRIG_PREC);
	if (270 == deg)
		return (FR_TRIG_MINVAL<<FR_TRIG_PREC);

	if (deg >= 0)
		return   deg <=  (90) ?  FR_TNDEG( deg ):-FR_TNDEG(180-deg);		
	else
		return   deg >= (-90) ? -FR_TNDEG(-deg ): FR_TNDEG(180+deg);

}	
/* Tan with s15.16 result with fixed radix input precision, 
   returns interpolated s15.16 result in degrees   
 */
 
s32 FR_tanDeg (s16 deg, u16 radix)
{
	s16 i,j;
	i = FR_TanI(deg>>radix);
	j=  FR_TanI((deg>>radix)+1);
	return FR_INTERPI(i,j,deg,radix);/*i+(((j-i)*(deg&((1<<radix)-1)))>>radix);*/
}	

/* Inverse Trig
 * Ugly looking acos with bin search (working):
 */
s16 FR_acosDeg (s32 input, u16 radix)
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

s16 FR_asinDeg (s32 input, u16 radix)
{
	return 90-FR_acos(input,radix);	 
}

s16 FR_atan2Deg (s32 y, s32 x, u16 radix)
{
	int q;
	if (0==x)
		return y < 0 ? FR_TRIG_MINVAL : FR_TRIG_MAXVAL;
	
	q = (y >= 0) ? ( (x>= 0) ? 0 : 1) : ((x>=0) ? 3 : 2);
	return q;
}

