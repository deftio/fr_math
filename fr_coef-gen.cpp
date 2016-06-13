/**
 *	@file FR_coef.cpp - coef gen for fix radix
 *  This program is intended to be run on a 32bit machine to generate coef. Not for embedded use		
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
#include "math.h"

#define LCL_PI (3.14159265358)
#define DECL  1
#define MACRO 2

int FR_Hibit(int n) {
	int x=0;
		while(n>>=1)
			x++;
    return x;
}
void gen_coef(const char* name, double x,int prec, int type=MACRO)
{
	double scale = (double)(1<<(prec));
	unsigned long 	n = (long)(scale * x);
	char	coef[32];
	int i,l=14-strlen(name);
	if (DECL == type)
	{
		printf("char %s[] ",name);
		for (i=0; i<l; i++)
			printf(" ");
		printf("= {");
		for (i=31;i>=16; i--)
			if (n&(1<<i))
				printf("%d,",i-15);
		printf("%d,",0);		
		for (i=15;i>=0; i--)
			if (n&(1<<i))
				printf("%d,",16-i);
		printf("0}; //13.9f\n",x);
	}
	if (MACRO==type)
	{
		printf("#define %s(x) ",name);
		char arr[32];
		int j=0,k=0;
		for (i=0; i<l; i++)
			printf(" ");
		printf("(");
		for (i=31;i>=16; i--)
			if (n&(1<<i))
			{
				arr[j]=(i-16);
				j++;
			}
		k=j;				
		for (i=15;i>=0; i--)
			if (n&(1<<i))
			{
				arr[j]=(16-i);
				j++;
			}
			
		for (i=0; i<(j-1); i++)
		{
			if (i<k)
				printf("((x)<<%d)",arr[i]);
			else
				printf("((x)>>%d)",arr[i]);
			if (j>1)
				printf("+");
		}
		printf("((x)>>%d))",arr[j-1]);
		
		printf(")  //%13.9f\n",x);
		
	}
}
	
void test_c (const char* coef)
{
	double x=0;

	while (*coef)
		x+= (double)(1<<((*coef++)-1));
	coef++;
	while (*coef)
		x+= (double)((1<<15)>>(*coef++))/((double)(1<<15));
	printf("%14.7f\n",x);
}
void gen_Constant_MACRO(const char *name,double val, int prec)	
{
	int i,l=16-strlen(name);
	printf("%s",name);
	for (i=0; i<l; i++)
		printf(" ");
	printf("(%d)   ",(long)((val*(double(1<<prec)))))	;
	printf("/* %13.8f   */\n",val);
}

#define DEGREES (1)
#define RADIANS (0)
#define COS	    (0)
#define SIN	    (1)
#define TAN	    (2)
void gen_trigTable(const char *tabname, double start, double end, double inc, long prec, int fun=COS, int rad_or_deg = DEGREES)
{
	double x,y;
	int k=0;
	int b = (rad_or_deg == DEGREES) ? 10 : 8;
	if (fun != TAN)
		printf("s16 const static %s[]={\n  ",tabname);
	else
		printf("s32 const static %s[]={\n  ",tabname);
	for (x = start; x<=end; x+= inc)
	{
		y = (rad_or_deg == DEGREES) ? x*2*LCL_PI/360.0 : x; 
		switch (fun)
		{
		case TAN:
			y=tan(y);
			break;
		case SIN:
			y=sin(y);
			break;
		default:
			y=cos(y);
		}
		y*=(((double)(1<<prec))-1);
		if(rad_or_deg == RADIANS)
		{
			if (fun == TAN)
				printf("0x%04x",(long)(y));
			else
				printf("0x%04x",(long)(y));
		}
		else
			printf("%7d",(long)(y));
		if ((x+inc) <=end)
			printf(", ");
		if ((k++)%(b)==(b-1))
			printf("\n  ");
	}
	printf("};\n");
	printf("#define %s_SZ     (%d)\n",tabname,k);
	if(!(k&(k-1)))
	{
		printf("#define %s_SZPREC (%d)\n",tabname,FR_Hibit(k));
		printf("#define %s_SZMASK (0x%x)\n\n",tabname,k-1);
	}
}
int main (void)
{
	int prec = 16;
	gen_coef("coef_r2d",57.29577951308232087679,prec);
	gen_coef("coef_e",2.718281828459045235360,prec);
	gen_coef("coef_r_e",0.367879441171442321595,prec);
	gen_coef("coef_pi",3.141592653589793238462,prec);
	gen_coef("coef_r_pi",0.318309886183790671537,prec);
	gen_coef("coef_r_log2_e",0.69314718056,prec);
	gen_coef("coef_d2r",0.017453292519943295769,prec);

	gen_Constant_MACRO("FR_krPI",3.141592653,prec);
	gen_Constant_MACRO("FR_kRAD2B128",40.743665431525,prec);
	gen_Constant_MACRO("FR_kB1282RAD",0.0245436926061,prec);
	gen_trigTable("gFR_COS_TAB_DEG_S0d15",0,89.9,1,15,COS,DEGREES);
	gen_trigTable("gFR_TAN_TAB_DEG_S0d15",0,89.1,1,15,TAN,DEGREES);
	double rad_inc = (LCL_PI/2.0)/128.0;
	gen_trigTable("gFR_COS_TAB_RAD_S0d15",0,(LCL_PI/2.0),rad_inc,15,COS,RADIANS);
	gen_trigTable("gFR_TAN_TAB_RAD_S0d15",0,(LCL_PI/4.0)+rad_inc,rad_inc,15,TAN,RADIANS);
}

