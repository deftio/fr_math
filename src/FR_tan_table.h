/**
 *  @file FR_tan_table.h - tangent quadrant tables (u32, s15.16)
 *
 *  Master table: gFR_TAN_TAB_Q[129]
 *      129 entries covering [0, pi/2] in s15.16 fixed-point.
 *      table[i] = round(tan(i * pi/2 / 128) * 65536), i=0..127
 *      table[128] = 0x7FFFFFFF (pole saturation)
 *      7-bit index + 7-bit lerp from 14-bit in-quadrant BAM.
 *
 *  Used by:
 *    fr_tan_bam32():  entries 0-64 directly (first octant, 0°-45°)
 *    fr_atan_bam32(): all 129 entries for binary-search arctangent
 *
 *  Second-octant variable-radix tables (derived from entries 64-128):
 *    gFR_TAN_MANT_Q2[65]:  u16 mantissa (top 16 bits)
 *    gFR_TAN_SHIFT_Q2[65]: u8 shift (bits to left-shift)
 *    Used by fr_tan_bam32() for division-free 45°-90° path.
 *
 *  Total ROM: 129×4 + 65×2 + 65×1 = 711 bytes
 *
 *  @copy Copyright (C) <2001-2026>  <M. A. Chatterjee>
 *  @author M A Chatterjee <deftio [at] deftio [dot] com>
 *
 *  Same zlib license as the rest of the library.
 */
#ifndef __FR_TAN_TABLE_H__
#define __FR_TAN_TABLE_H__

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __FR_Platform_Defs_H__
#include "FR_defs.h"
#endif

/* ── 129-entry table (used by atan binary search) ───────── */
#define FR_TAN32_TABLE_BITS   (7)
#define FR_TAN32_TABLE_SIZE   ((1 << FR_TAN32_TABLE_BITS) + 1)  /* 129 */
#define FR_TAN32_FRAC_BITS    (14 - FR_TAN32_TABLE_BITS)        /* 7   */
#define FR_TAN32_FRAC_MAX     (1 << FR_TAN32_FRAC_BITS)         /* 128 */
#define FR_TAN32_FRAC_MASK    (FR_TAN32_FRAC_MAX - 1)           /* 0x7F */
#define FR_TAN32_FRAC_HALF    (FR_TAN32_FRAC_MAX >> 1)          /* 64  */
#define FR_TAN32_QUADRANT     (1 << 14)                          /* 16384 */

static const u32 gFR_TAN_TAB_Q[FR_TAN32_TABLE_SIZE] = {
           0,       804,      1609,      2414,
        3220,      4026,      4834,      5644,
        6455,      7268,      8083,      8901,
        9721,     10545,     11372,     12202,
       13036,     13874,     14717,     15564,
       16416,     17273,     18136,     19005,
       19880,     20762,     21650,     22546,
       23449,     24360,     25280,     26208,
       27146,     28093,     29050,     30018,
       30996,     31986,     32988,     34002,
       35030,     36071,     37126,     38196,
       39281,     40382,     41500,     42636,
       43790,     44963,     46156,     47369,
       48605,     49863,     51145,     52451,
       53784,     55144,     56532,     57950,
       59398,     60880,     62395,     63947,
       65536,     67165,     68835,     70548,
       72308,     74116,     75974,     77887,
       79856,     81885,     83977,     86135,
       88365,     90670,     93054,     95523,
       98082,    100736,    103493,    106358,
      109340,    112447,    115687,    119071,
      122609,    126314,    130198,    134276,
      138564,    143081,    147847,    152884,
      158218,    163878,    169896,    176309,
      183161,    190499,    198380,    206870,
      216043,    225990,    236817,    248648,
      261634,    275959,    291845,    309568,
      329472,    351993,    377693,    407305,
      441808,    482534,    531352,    590958,
      665398,    761030,    888450,   1066730,
     1334016,   1779314,   2669641,   5340086,
  2147483647
};

/* ── Second-octant variable-radix tables (used by forward tan) ── */

/* Mantissa table: top 16 bits of gFR_TAN_TAB_Q[64..128].
 * gFR_TAN_MANT_Q2[i] = gFR_TAN_TAB_Q[64+i] >> gFR_TAN_SHIFT_Q2[i]
 * 65 entries × 2 bytes = 130 bytes ROM.
 */
static const u16 gFR_TAN_MANT_Q2[65] = {
    32768, 33582, 34417, 35274, 36154, 37058, 37987, 38943,
    39928, 40942, 41988, 43067, 44182, 45335, 46527, 47761,
    49041, 50368, 51746, 53179, 54670, 56223, 57843, 59535,
    61304, 63157, 65099, 33569, 34641, 35770, 36961, 38221,
    39554, 40969, 42474, 44077, 45790, 47624, 49595, 51717,
    54010, 56497, 59204, 62162, 65408, 34494, 36480, 38696,
    41184, 43999, 47211, 50913, 55226, 60316, 33209, 36934,
    41587, 47564, 55528, 33335, 41688, 55603, 41713, 41719,
    65535
};

/* Shift table: bits to left-shift mantissa to reconstruct s15.16 value.
 * 65 entries × 1 byte = 65 bytes ROM.
 */
static const u8 gFR_TAN_SHIFT_Q2[65] = {
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,
     1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  2,
     2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  3,  3,  3,
     3,  3,  3,  3,  3,  3,  4,  4,  4,  4,  4,  5,  5,  5,  6,  7,
    15
};

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* __FR_TAN_TABLE_H__ */
