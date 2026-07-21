/**
 * test_arduino_compat.c - Arduino environment compatibility tests (issue #11)
 *
 * Verifies that FR_defs.h provides u8/u16/u32 (and friends) in every
 * Arduino build flavor.  The Makefile target test-arduino-compat compiles
 * this file several ways to simulate the environments that matter:
 *
 *   1. -DARDUINO, C++            : ESP32/RP2040/STM32-style core — no
 *                                  USBAPI.h typedefs exist; FR_defs.h must
 *                                  supply u8/u16 (the issue #11 failure).
 *   2. -DARDUINO, C++,
 *      -DFR_TEST_USBAPI_FIRST    : AVR/SAM/SAMD-style core — USBAPI.h has
 *                                  already typedef'd u8/u16 before our
 *                                  header is included; must not clash.
 *   3. -DARDUINO, C++,
 *      -DFR_TEST_USBAPI_AFTER    : FR_math.h included before Arduino.h;
 *                                  USBAPI.h typedefs come second.
 *   4. -DARDUINO, plain C        : Arduino builds of FR_math.c; USBAPI.h
 *                                  never appears in C translation units.
 *   5. no -DARDUINO, C and C++   : host builds, unchanged behavior.
 *
 * This file is written in the common subset of C89 and C++ so the same
 * source compiles in every mode above.
 */

/* Simulated ArduinoCore-avr USBAPI.h typedefs (verbatim underlying types) */
#if defined(FR_TEST_USBAPI_FIRST) && defined(__cplusplus)
typedef unsigned char u8;
typedef unsigned short u16;
#endif

#include "FR_math.h"

#if defined(FR_TEST_USBAPI_AFTER) && defined(__cplusplus)
typedef unsigned char u8;
typedef unsigned short u16;
#endif

#include <stdio.h>

/* Compile-time checks (C89-compatible negative-array-size trick) */
typedef char fr_ct_u8_is_1_byte[(sizeof(u8) == 1) ? 1 : -1];
typedef char fr_ct_u16_is_2_bytes[(sizeof(u16) == 2) ? 1 : -1];
typedef char fr_ct_u32_is_4_bytes[(sizeof(u32) == 4) ? 1 : -1];
typedef char fr_ct_u64_is_8_bytes[(sizeof(u64) == 8) ? 1 : -1];
typedef char fr_ct_u8_unsigned[((u8)-1 > 0) ? 1 : -1];
typedef char fr_ct_u16_unsigned[((u16)-1 > 0) ? 1 : -1];
typedef char fr_ct_u32_unsigned[((u32)-1 > 0) ? 1 : -1];
typedef char fr_ct_s16_signed[((s16)-1 < 0) ? 1 : -1];
typedef char fr_ct_s32_signed[((s32)-1 < 0) ? 1 : -1];

/* The u16-typed public API must be declared and callable (issue #11 was a
   failure to even parse these prototypes on ESP32). */
static s32 use_u16_api(void)
{
    u16 bam = FR_DEG2BAM_I(45);
    return fr_sin_bam(bam) + fr_cos_bam(bam);
}

int main(void)
{
    int fails = 0;

    if (sizeof(u8) != 1 || sizeof(u16) != 2 || sizeof(u32) != 4) {
        printf("  type sizes: FAIL\n");
        fails++;
    } else {
        printf("  type sizes (u8=1, u16=2, u32=4): PASS\n");
    }

    if (use_u16_api() == 0) {
        /* sin(45)+cos(45) in s0.15 is decidedly nonzero */
        printf("  u16 BAM API callable: FAIL\n");
        fails++;
    } else {
        printf("  u16 BAM API callable: PASS\n");
    }

    printf("Arduino compat: %s\n", fails ? "FAIL" : "ALL PASS");
    return fails;
}
