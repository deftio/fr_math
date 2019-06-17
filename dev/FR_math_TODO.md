# FR_Math TODO 
[![Build Status](https://travis-ci.org/deftio/fr_math.svg?branch=master)](https://travis-ci.org/deftio/fr_math)
[![Coverage Status](https://coveralls.io/repos/github/deftio/fr_math/badge.svg?branch=master)](https://coveralls.io/github/deftio/fr_math?branch=master)

(c) M. A. Chatterjee 2000-2016

# Features
	[x] tanI 
	[ ] tanFR version
	[ ] acos
	[ ] asin
	[ ] atan
	[ ] atan2
* (done) degrees-->radians (macro (up to) 16bit FR)
* (done) radians-->degrees (macro (up to) 16bit FR)
* (done) macros for common constants
* (done) add/sub of different radixes as functions/macros
* rename FR_main_examples_and_test_suite.cpp
* rename integer trig functions to FR_cos_i FR_sin_i  or FR_cosD, FR_sinD (for degrees)
* move degree based trig to separate FR_TrigDegrees (all fns)
* add JSON output for error measurement
* add simple HTML error renderer
* rename FR_Sin --> FR_sin  
* use radians instead of degrees?  
	* provide sep compile flags for radians vs degrees for quads
* Provide macros for sin / cos in degrees?  or just example FR_SIN(FRDEG2RAD(x))
* Provide wave functions:
* FR_squareWavS //square wave of sin
* FR_squareWavC //square wave of cos
* FR_triangleWavS //triangle wav of sin
* FR_triangleWavC //triangle wav of cos
* FR_pwm(radians,duty) // starts hi goes low after duty percent of cycle, duty on scale of 0..16bits
* need interp 
	FR_Interp(x, y0, y1, FR_INTERP_TYPE) // interp types: FR_INTP_FLOOR, FR_INTERP_CEIL, FR_INTERP_NN, FR_INTERP_LIN, FR_INTERP_COS
* test suite
* printout out numbers e.g. FR_math_print((int *)f(char *),  s32 num, int padding, dec);


scale by Pi, e and their reciprocals by shifting
#define FR_SMUL10(x)	(((x)<<3)+((x<<1)))
#define FR_SPI(x)       (((x)<<1)+(x)+(x>>3)+(x>>6)+(x>>10))
#define FR_SrPI(x)      ((x>>2)+(x>>4)+(x>>8)+(x>>9)-(x>>14)+(x>>16))

Scale by a constant, by reciprocal
FR_SE
FR_SrE

make better explanations in FR_math_docs.md

Note on assumption of incremental linker usage

TODO examples:
* saturation
* switching radixes
* comparing to floating point
* why this libraries allows _arbitrary_ radix not just on byte boundaries
* why degrees were used and not radians and not binary degrees (e.g. 64 pts for a quadrant etc)

notes on that all functions are stable (e.g. no div by zero even tan, acos etc).

separate test suite and example programs
* test for arcxxx functions to work with different radixes eg 5 (etc)

* add BitsInUse (a function which brackets highest and lowest set bits and their positions.  Note proper handling of negative numbers)
* add functions/macros for using the FR_kXXX constants eg FR_kScale(x,prec,FR_kCONST), FR_kADD(x,prec,FR_kCONST)
* move integer trig functions to macros
* make sin(), MACROs
* make arcsin MACRO

sqrt?
1/sqrt --> inv_sqrt
1/x
FR interpolation macro?
Log2?
exp?
pow(x,y) --> pow2(y*log2(x))
fr_pow(x,xr,y,yr) --> pow2(FR_CHRDX(y*log2(x,xr),xr),yr)

Add FR_INTERP_COS()  //MACRO!!
    double CosineInterpolate(
       double y1,double y2,  double mu)
    {
       double mu2;

       mu2 = (1-cos(mu*PI))/2;
       return(y1*(1-mu2)+y2*mu2);
    }
    ==>  //#define FR_INTERPI(x0,x1,delta,prec) ((x0)+((((x1)-(x0))*((delta)&((1<<(prec))-1)))>>(prec)))
    FR_INTERP_COSI(x0,x1,delta,prec) // interp cosine, interval
    {
        mu = I2FR(1,prec)-FR_COS(FR_sPI(delta))>>1; //mu = (1-FR_COS(delta*pi))>>1; 
        return FR_INTERPI(
    }

clean up terminology (e.g. signed vs sat suffixes)
make all standalone functions lowercase except for FR_ prefix to match math.h type libs
add text on differences between fixed_radix, BCD, floating point 32 double, and bignum math systems
example code on how to use

(done) add a namespace under C++

# Future Features
Add, Mul, Sub (with Sat) on arbitrary long #s
? Conversions to Half precision FP (e.g. 11.5)
? Support for S3.12 FP?

#================================================
NOTES NOTES NOTES NOTES NOTES NOTES
#================================================
// Hibit keeps highest bit set, doesn't return the actual postion as a number
int FR_Hibit(s32 n) {
    n |= (n >>  1);
    n |= (n >>  2);
    n |= (n >>  4);
    n |= (n >>  8);
    n |= (n >> 16);
    return n - (n >> 1);
}
#================================================
int hibit(s32 n) {
	int x=1;
	while ((x<<=1)<32)
    	n |= (n >>  x);
    return n - (n >> 1);
}
#================================================
s32 FR_Log2 (s32 x, s32 s)
{
	s32 j, v=x, f=0; 
	for (j=16; j>0; j>>=1)/*perform binary search for the highest set bit*/
		if ( x>(1<<j)){f+=j; x>>=j;}

	//Next line is variable precision linear interpolation with offset.
	return (f<<s)+(1<<s>>5)+((f>s)?((v+(1<<f))>>(f-s)):(v+(1<<f)<<(s+f)));

}
///*Next line converts from Log2(x) to Ln by computing log2(x)/log2(e).*/
/// return (v>>1)+(v>>2)-(v>>4)+(v>>7)-(v>>9)-(v>>12)+(v>>15);

Log2 can be improved with cos interpolation or acceleration interpolation.  or log2 again of the sub-interval followed by linear
see also ... http://codeplea.com/simple-interpolation


 

#================================================
//below works (9 terms, 0.00001 error)
#define FR_DEG2RAD(x) (((x)<<6)-((x)<<3)+(x)-1+((x)>>2)+((x)>>4)+((x)>>5)+((x)>>6)-((x)>>9)+((x)>>10)-((x)>>13))
#define FR_DEG2RAD(x) (((x)<<6)-((x)<<3)+(x)-1+((x)>>2)+((x)>>4)-((x)>>6)-((x)>>9)+((x)>>11)+((x)>>12)+((x)>>13))

5142 before TanI added
5232 with TanI 

#================================================

//Ugly looking acos with bin search (working):
//Inverse Trig
s16 FR_acos (s32 input, u16 radix)
{
	s16 r=45, s=input, x=46,y,z;	
	//radix normalization to s0.15
	if (radix < FR_TRIG_PREC)
		input<<= (FR_TRIG_PREC-radix);
	if (radix > FR_TRIG_PREC)
		input>>= (radix-FR_TRIG_PREC);
	// +or- 1.0000 is special case as it doesn't fit in table search 
	if ((input&0xffff)==0x8000) 
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

#================================================
//simple linear search arccos (works), input in degrees
s16 FR_acos (s32 input, u16 radix)
{
	s16 r=0, s=input, x=gFR_COS_TAB_S0d15[0];
	//radix normalization to cos table precision
	/*if (radix < FR_TRIG_PREC)
		input<<= (FR_TRIG_PREC-radix);
	if (radix > FR_TRIG_PREC)
		input>>= (radix-FR_TRIG_PREC);
		*/
	input = FR_CHRDX(input,radix,FR_TRIG_PREC);
	// +or- 1.0000 is special case as it doesn't fit in table search 
	if ((input&0xffff)==0x8000) 
		return (input<0) ? 180: 0;
	
	input = (FR_ABS(input)) & ((1<<radix)-1);
	while (x>input)
		x=gFR_COS_TAB_S0d15[r++];
	
	r = FR_ABS(input - gFR_COS_TAB_S0d15[r])< FR_ABS(input-x)?r:r-1; 
	return  (s>0) ? r:180-r;	 
}
#================================================
#================================================
Detect if two integers have opposite signs
int x, y;               // input values to compare signs
bool f = ((x ^ y) < 0); // true iff x and y have opposite signs
Manfred Weis suggested I add this entry on November 26, 2009.
#================================================
s32 mod360(s32 x)
{
  s32 y,div360  = 1509949440; // this constant is (1<<log2_floor(MAX_INT/360))*360
  y = x<0 ? -x : x;  
  while (div360 >= 360)
  {
     if (y >= div360) y -= div360; 
     div360 >>=1;  
     printf("*");
  }
  printf("\n");
  return x>0 ? y : -y;
}
main()
{
    int i=0;
    for (i=-234324; i< 323342; i+=817)
    {
        printf("%6d %6d %6d\n",i,i%360,mod360(i)); //is this doublne slash cmt ok?
    }
}
#================================================
//working FR_cosI
s16 FR_CosI(s16 deg) 
{
	deg = ((deg%360)+360)%360; /* this is an expensive operation */
	if (deg<180)
		return   deg<90 ? gFR_COS_TAB_S0d15[deg    ]:-gFR_COS_TAB_S0d15[180-deg];		
	else
		return -(deg<270? gFR_COS_TAB_S0d15[deg-180]:-gFR_COS_TAB_S0d15[360-deg]);	
}
#================================================

#================================================
s16 FR_SquareWavSI(s16 deg) 
{
	deg = deg%360; /* this is an expensive operation*/
	if (deg > 180) { deg -= 360; }
	else if (deg < -180) { deg += 360;}
	
	return (deg >= 0) ? 32767 : (-32767);
}
s16 FR_SquareWavCI(s16 deg) 
{
	return FR_SquareWavCI(deg+90);
}


#============================================================
/* Cosine table in s0.15 format, 1 entry per 1/nth of a quadrant
 * used in all trig functions (sin,cos,tan, atan2 etc)
 */
s16 const static gFR_QCOS_TAB_S0d15[]={
   .};
    
/* cosine with integer input precision in degrees, returns s0.15 result
 */

s16 FR_cosineQ(s32 q, u16 r) 
{
	s16 i = FR_INT(q,r)&0x3;
    u16 f = FR_FRACS(q,r,7);
    
	if (i < 2)
		return   (0==i) ?  gFR_QCOS_TAB_S0d15[ f ]:-gFR_QCOS_TAB_S0d15[0x80-f];		
	else
		return   (3==i) ? -gFR_QCOS_TAB_S0d15[ f ]: gFR_QCOS_TAB_S0d15[0x80-f];	
}

s16 FR_squareWaveSinQ(s32 q, u16 r) 
{
	return (FR_INT(q,r)&0x3 < 2) ? 32767 : -32767;    
}

s16 FR_squareWaveCosQ(s32 q, u16 r) 
{
	return ((2+FR_INT(q,r))&0x3 < 2) ? 32767 : -32767;    
}