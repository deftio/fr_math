/**
 *	@file FR_Defs.h - type definitions used in Fixed-Radix math lib
 *
 *	@copy Copyright (C) <2001-2012>  <M. A. Chatterjee>
 *  @author M A Chatterjee <deftio [at] deftio [dot] com>
 *	@version 1.0.3 M. A. Chatterjee, cleaned up naming
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

#ifndef __FR_Platform_Defs_H__
#define __FR_Platform_Defs_H__

typedef unsigned char u8;
typedef signed char s8;
typedef unsigned short u16;
typedef signed short s16;
typedef unsigned long u32;
typedef signed long s32;

typedef short FR_bool;
typedef u16 FR_RESULT;

#define FR_SWAP_BYTES(x) (((x >> 8) & 0xff) | ((x << 8) & 0xff00))

// Return codes

#ifdef WIN32
#pragma warning(disable : 4001 4514)
#endif

// generic error codes
#define FR_S_OK (0x0000)

#define FR_E_FAIL (0x8000)
#define FR_E_BADARGUMENTS (0x8001)
#define FR_E_NULLPOINTER (0x8002)
#define FR_E_INDEXOUTOFRANGE (0x8003)
#define FR_E_BUFFERFULL (0x8004)
#define FR_E_NOTIMPLEMENTED (0x8005)
#define FR_E_MEMALLOCFAILED (0x8006)
#define FR_E_UNKNOWNOBJECT (0x8007)

// math specific
#define FR_E_UNABLE (0x8100)

#define FR_FAILED(x) (x & 0x8000)
#define FR_SUCCEEDED(x) (!(x & 0x8000))

/*******************************/
#define FR_FALSE (0)
#define FR_TRUE (!FR_FALSE)

#endif // __FR_Platform_Defs_H__
