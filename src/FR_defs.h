/**
 *	@file FR_Defs.h - type definitions used in Fixed-Radix math lib
 *
 *	@copy Copyright (C) <2001-2026>  <M. A. Chatterjee>
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

#ifndef __FR_Platform_Defs_H__
#define __FR_Platform_Defs_H__

/*
 * Fixed-width integer typedefs. C99 stdint.h is mandatory in v2.
 *
 * Any C99-or-newer toolchain (gcc, clang, MSVC, IAR, Keil, sdcc, MSP430-gcc,
 * AVR-gcc, RISC-V toolchains, ARM toolchains) supports <stdint.h>. If you
 * are on a pre-C99 toolchain, FR_Math 1.0.x is the version for you.
 */
#include <stdint.h>

/*
 * Arduino's USBAPI.h typedefs u8 and u16 as unsigned char / unsigned short.
 * On AVR, uint8_t/uint16_t resolve to unsigned int types, which are the same
 * width but different C++ types — causing a redefinition error.  Skip those
 * two typedefs when building in an Arduino environment; the Arduino-provided
 * types are the same width and work identically.
 *
 * The guard checks __cplusplus too because Arduino.h (which pulls in
 * USBAPI.h) is only auto-included in .ino/.cpp translation units.
 * Plain-C files (.c) compiled by the Arduino build system need our
 * typedefs even though the ARDUINO macro is defined for them.
 */
#if defined(ARDUINO) && defined(__cplusplus)
  /* Arduino C++ TU — USBAPI.h already provides u8 and u16 */
#else
typedef uint8_t  u8;
typedef uint16_t u16;
#endif
typedef int8_t   s8;
typedef int16_t  s16;
typedef uint32_t u32;
typedef int32_t  s32;
typedef uint64_t u64;
typedef int64_t  s64;

typedef short FR_bool;

#define FR_SWAP_BYTES(x) (((x >> 8) & 0xff) | ((x << 8) & 0xff00))

/*=======================================================
 * Sentinel values for math errors.
 *
 * Functions that can hit a domain error (sqrt of negative, log of <=0, etc.)
 * return one of these sentinels rather than raising an error code. Callers
 * that care can check `result == FR_DOMAIN_ERROR` before using the value.
 *
 * FR_OVERFLOW_POS and FR_OVERFLOW_NEG are the saturating-overflow values
 * returned by FR_FixMulSat / FR_FixAddSat etc. They are deliberately the
 * extremes of s32 so that they compare correctly under signed comparison.
 *
 * Note: FR_DOMAIN_ERROR equals INT32_MIN (the same value as FR_OVERFLOW_NEG
 * would naturally be), so a single check `result <= FR_OVERFLOW_NEG` cannot
 * distinguish "saturating-negative-overflow" from "domain error". In
 * practice the two never occur in the same call: saturating ops never go
 * to a domain-error state, and domain-error ops never saturate. The names
 * exist to make caller intent self-documenting.
 */
#define FR_DOMAIN_ERROR  ((s32)0x80000000) /* INT32_MIN: e.g. sqrt(<0)        */
#define FR_OVERFLOW_POS  ((s32)0x7fffffff) /* INT32_MAX: e.g. FR_FixMulSat +  */
#define FR_OVERFLOW_NEG  ((s32)0x80000000) /* INT32_MIN: e.g. FR_FixMulSat -  */

#define FR_FALSE (0)
#define FR_TRUE (!FR_FALSE)

#endif /* __FR_Platform_Defs_H__ */
