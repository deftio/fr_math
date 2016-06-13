#!/usr/bin/env python
"""
	coef-gen.py -  simple fixed point power-of-2 expression & coef generator
	
	@copy Copyright (C) <2012>  <M. A. Chatterjee>
	
	@author M A Chatterjee, deftio [at] deftio [dot] com
	
	This software is provided 'as-is', without any express or implied
	warranty. In no event will the authors be held liable for any damages
	arising from the use of this software.

	Permission is granted to anyone to use this software for any purpose,
	including commercial applications, and to alter it and redistribute it
	freely, subject to the following restrictions:

	1. The origin of this software must not be misrepresented; you must not
	claim that you wrote the original software. If you use this software
	in a product, an acknowledgment in the product documentation would be
	appreciated but is not required.

	2. Altered source versions must be plainly marked as such, and must not be
	misrepresented as being the original software.

	3. This notice may not be removed or altered from any source
	distribution.

"""

import math

def log2(x):
	return math.log(x)/math.log(2)
	
def pow2(x):
	return pow(2.0,x)

def test(vect):
	if len(vect) == 0:
		return 0
	sumv = 0
	for i in range(len(vect)):
		if vect[i] < 0:
			sumv = sumv - pow2(vect[i])
		elif vect[i] > 0:
			sumv = sumv + pow2(vect[i])
	return sumv
		
def genCoef(n, max_power = -16):
	x = math.floor(log2(n))
	err = n-x
	z = x-1
	v = [int(x)]
	while (z >= max_power):
		newterm = 0
		ep = abs(n-(x+pow2(z)))
		if ep < err:
			newterm = z
		en = abs(n-(x-pow2(z)))
		if en < err:
			newterm = -z
		
		if newterm !=0:
			v.append(int(newterm))
		z = z -1
	print v
	print len(v)
	print n
	print test(v)

def main():
	genCoef(3.141592653)
	
	
	
if __name__ == '__main__':
    main()
