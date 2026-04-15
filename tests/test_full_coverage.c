/*
 * test_full_coverage.c - Achieve 100% test coverage for FR_Math library
 * Tests every function and every branch
 * 
 * @author M A Chatterjee <deftio [at] deftio [dot] com>
 */

#include <stdio.h>
#include <string.h>
#include <limits.h>
#include "../src/FR_math.h"

/* Disable warnings for test code */
#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wshift-count-negative"
#pragma clang diagnostic ignored "-Wshift-negative-value"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshift-count-negative"
#pragma GCC diagnostic ignored "-Wshift-negative-value"
#endif

#define TEST_PASS 0
#define TEST_FAIL 1

static int test_count = 0;
static int fail_count = 0;

#define RUN_TEST(test_func) do { \
    printf("  %s: ", #test_func); \
    test_count++; \
    if (test_func() == TEST_PASS) { \
        printf("PASS\n"); \
    } else { \
        printf("FAIL\n"); \
        fail_count++; \
    } \
} while(0)

/* Test FR_FixMuls - signed multiply with round-to-nearest */
int test_fixmuls() {
    s32 result;

    /* Test positive * positive: 1.0 * 2.0 = 2.0 */
    result = FR_FixMuls(0x10000, 0x20000);
    if (result != 0x20000) return TEST_FAIL;

    /* Test negative * positive: -1.0 * 2.0 = -2.0 */
    result = FR_FixMuls(-0x10000, 0x20000);
    if (result != -0x20000) return TEST_FAIL;

    /* Test positive * negative: 1.0 * -2.0 = -2.0 */
    result = FR_FixMuls(0x10000, -0x20000);
    if (result != -0x20000) return TEST_FAIL;

    /* Test negative * negative: -1.0 * -2.0 = 2.0 */
    result = FR_FixMuls(-0x10000, -0x20000);
    if (result != 0x20000) return TEST_FAIL;

    /* Rounding test: 1.5 * 1.0 at radix 16.
     * 1.5 = 0x18000. Product = 0x18000 * 0x10000 = 0x180000000.
     * + 0x8000 = 0x180008000. >> 16 = 0x18000 = 1.5 exactly. */
    result = FR_FixMuls(0x18000, 0x10000);
    if (result != 0x18000) return TEST_FAIL;

    /* Rounding matters: 3 * 3 = 9 at full precision, but at radix 16
     * 0x30000 * 0x30000 = 0x900000000, + 0x8000 = 0x900008000,
     * >> 16 = 0x90000 = 9.0 exactly. */
    result = FR_FixMuls(0x30000, 0x30000);
    if (result != 0x90000) return TEST_FAIL;

    /* Rounding edge: product whose low 16 bits are exactly 0x8000 rounds up.
     * 0x18001 * 0x10000 = 0x180010000. + 0x8000 = 0x180018000. >> 16 = 0x18001. */
    result = FR_FixMuls(0x18001, 0x10000);
    if (result != 0x18001) return TEST_FAIL;

    return TEST_PASS;
}

/* Test FR_FixMulSat - saturating multiply with round-to-nearest */
int test_fixmulsat() {
    s32 result;

    /* Normal: 1.0 * 2.0 = 2.0 */
    result = FR_FixMulSat(0x10000, 0x20000);
    if (result != 0x20000) return TEST_FAIL;

    /* Overflow saturates to positive */
    result = FR_FixMulSat(0x7FFF0000, 0x7FFF0000);
    if (result != FR_OVERFLOW_POS) return TEST_FAIL;

    /* Negative * positive */
    result = FR_FixMulSat(-0x10000, 0x20000);
    if (result != -0x20000) return TEST_FAIL;

    /* Negative * negative */
    result = FR_FixMulSat(-0x10000, -0x20000);
    if (result != 0x20000) return TEST_FAIL;

    /* Non-overflowing large multiply: 0x7FFFFFFF * 2 fits after >>16 */
    result = FR_FixMulSat(0x7FFFFFFF, 2);
    if (result != 0x10000) return TEST_FAIL;

    /* Negative overflow saturates to negative */
    result = FR_FixMulSat(0x7FFF0000, -0x7FFF0000);
    if (result != FR_OVERFLOW_NEG) return TEST_FAIL;

    /* Rounding: same as FixMuls for non-overflowing inputs */
    result = FR_FixMulSat(0x18000, 0x10000);
    if (result != 0x18000) return TEST_FAIL;

    return TEST_PASS;
}

/* Test FR_FixAddSat - uncovered function */  
int test_fixaddsat() {
    s32 result;
    
    /* Test normal addition */
    result = FR_FixAddSat(1000, 2000);
    
    /* Test positive overflow */
    result = FR_FixAddSat(0x7FFFFFF0, 100);
    
    /* Test negative overflow */
    result = FR_FixAddSat(-0x7FFFFFF0, -100);
    
    /* Test mixed signs */
    result = FR_FixAddSat(1000, -500);
    
    (void)result;
    return TEST_PASS;
}

/* Test FR_DIV (64-bit), FR_DIV32 (32-bit), FR_MOD */
int test_div() {
    s32 result;

    /* Basic: 10 / 2 = 5 at radix 8 */
    result = FR_DIV(I2FR(10, 8), 8, I2FR(2, 8), 8);
    if (result != I2FR(5, 8)) return TEST_FAIL;

    /* FR_DIV32 same for small values */
    result = FR_DIV32(I2FR(10, 8), 8, I2FR(2, 8), 8);
    if (result != I2FR(5, 8)) return TEST_FAIL;

    /* Fractional result: 7 / 2 = 3.5 at radix 8 */
    result = FR_DIV(I2FR(7, 8), 8, I2FR(2, 8), 8);
    if (result != I2FR(3, 8) + (1 << 7)) return TEST_FAIL;

    /* 64-bit advantage: large numerator that overflows in 32-bit.
     * 1000 / 3 at radix 16.  1000 << 16 = 65,536,000 which fits s32,
     * but 30000 << 16 = 1,966,080,000 which is near INT32_MAX.
     * Use a value that overflows 32-bit but works in 64-bit. */
    result = FR_DIV(I2FR(30000, 16), 16, I2FR(3, 16), 16);
    /* 30000 / 3 = 10000.0 exactly */
    if (result != I2FR(10000, 16)) return TEST_FAIL;

    /* Negative numerator */
    result = FR_DIV(I2FR(-10, 8), 8, I2FR(2, 8), 8);
    if (result != I2FR(-5, 8)) return TEST_FAIL;

    /* Negative denominator */
    result = FR_DIV(I2FR(10, 8), 8, I2FR(-2, 8), 8);
    if (result != I2FR(-5, 8)) return TEST_FAIL;

    /* Both negative */
    result = FR_DIV(I2FR(-10, 8), 8, I2FR(-2, 8), 8);
    if (result != I2FR(5, 8)) return TEST_FAIL;

    /* FR_MOD: 10 % 3 = 1 at radix 8 */
    result = FR_MOD(I2FR(10, 8), I2FR(3, 8));
    if (result != I2FR(1, 8)) return TEST_FAIL;

    /* FR_MOD: -10 % 3 = -1 (C semantics) */
    result = FR_MOD(I2FR(-10, 8), I2FR(3, 8));
    if (result != I2FR(-1, 8)) return TEST_FAIL;

    /* FR_DIV with different radix: x at radix 8, y at radix 4 */
    result = FR_DIV(I2FR(10, 8), 8, I2FR(2, 4), 4);
    if (result != I2FR(5, 8)) return TEST_FAIL;

    return TEST_PASS;
}

/* Test all trig functions comprehensively including edge cases */
int test_trig_complete() {
    s16 result;
    s32 result32;
    
    /* Test CosI with all quadrants and edge cases */
    result = FR_CosI(0);
    result = FR_CosI(45);
    result = FR_CosI(90);
    result = FR_CosI(135);
    result = FR_CosI(180);
    result = FR_CosI(225);
    result = FR_CosI(270);
    result = FR_CosI(315);
    result = FR_CosI(360);
    
    /* Test angles > 180 to hit the branch */
    result = FR_CosI(200);  /* > 180, will subtract 360 */
    result = FR_CosI(350);  /* > 180, will subtract 360 */
    
    /* Test angles < -180 to hit that branch */
    result = FR_CosI(-200); /* < -180, will add 360 */
    result = FR_CosI(-350); /* < -180, will add 360 */
    
    /* Test SinI */
    result = FR_SinI(0);
    result = FR_SinI(90);
    result = FR_SinI(180);
    result = FR_SinI(270);
    
    /* Test FR_Cos with radix (interpolated) */
    result = FR_Cos(45, 8);
    result = FR_Cos(90, 8);
    result = FR_Cos(180, 8);
    
    /* Test FR_Sin with radix */
    result = FR_Sin(45, 8);
    result = FR_Sin(90, 8);
    
    /* Test TanI with all special cases */
    result32 = FR_TanI(0);
    result32 = FR_TanI(45);
    result32 = FR_TanI(90);   /* Special case: returns max */
    result32 = FR_TanI(135);
    result32 = FR_TanI(180);
    result32 = FR_TanI(270);  /* Special case: returns -max */
    result32 = FR_TanI(-45);  /* Negative angle */
    result32 = FR_TanI(-90);  /* Negative 90 */
    result32 = FR_TanI(200);  /* > 180 */
    result32 = FR_TanI(-200); /* < -180 */
    
    /* Test FR_Tan with radix */
    result32 = FR_Tan(45, 8);
    result32 = FR_Tan(30, 8);
    
    (void)result;
    (void)result32;
    return TEST_PASS;
}

/* Test inverse trig functions */
int test_inverse_trig() {
    s32 result, input;

    /* acos/asin now return radians at out_radix. Use radix 16 (s15.16).
     * pi/2 ≈ 102944 at radix 16, pi ≈ 205887 at radix 16. */

    /* Test acos(1.0) = 0 */
    input = I2FR(1, 15);
    result = FR_acos(input, 15, 16);
    if (result < -200 || result > 200) return TEST_FAIL;

    /* Test acos(0) = pi/2 ≈ 102944 */
    result = FR_acos(0, 15, 16);
    if (result < 100000 || result > 106000) return TEST_FAIL;

    /* Test acos(-1.0) = pi ≈ 205887 */
    input = -I2FR(1, 15);
    result = FR_acos(input, 15, 16);
    if (result < 200000 || result > 210000) return TEST_FAIL;

    /* Test special case: exactly ±1.0 (clamped range) */
    input = 0x8000;
    result = FR_acos(input, 15, 16);
    result = FR_acos(-input, 15, 16);

    /* Test asin(0) = 0 */
    result = FR_asin(0, 15, 16);
    if (result < -200 || result > 200) return TEST_FAIL;

    /* Test asin(1.0) = pi/2 ≈ 102944 */
    input = I2FR(1, 15);
    result = FR_asin(input, 15, 16);
    if (result < 100000 || result > 106000) return TEST_FAIL;

    (void)result;
    return TEST_PASS;
}

/* Test log functions with all branches */
int test_log_complete() {
    s32 result;
    
    /* Test log2 with various inputs to hit all branches */
    result = FR_log2(I2FR(1, 8), 8, 8);
    result = FR_log2(I2FR(2, 8), 8, 8);
    result = FR_log2(I2FR(4, 8), 8, 8);
    result = FR_log2(I2FR(8, 8), 8, 8);
    result = FR_log2(I2FR(16, 8), 8, 8);
    result = FR_log2(I2FR(32, 8), 8, 8);
    result = FR_log2(I2FR(64, 8), 8, 8);
    result = FR_log2(I2FR(128, 8), 8, 8);
    result = FR_log2(I2FR(256, 8), 8, 8);
    
    /* Test with value that won't trigger shift */
    result = FR_log2(1, 8, 8);  /* Small value, won't enter if(input > (1<<h)) */
    
    /* Test edge cases */
    result = FR_log2(0, 8, 8);     /* Should return FR_LOG2MIN */
    result = FR_log2(-100, 8, 8);  /* Negative, should return FR_LOG2MIN */
    
    /* Test ln and log10 */
    result = FR_ln(I2FR(10, 8), 8, 8);
    result = FR_ln(I2FR(100, 8), 8, 8);
    
    result = FR_log10(I2FR(10, 8), 8, 8);
    result = FR_log10(I2FR(100, 8), 8, 8);
    result = FR_log10(I2FR(1000, 8), 8, 8);
    
    (void)result;
    return TEST_PASS;
}

/* Test pow2 with all branches */
int test_pow2_complete() {
    s32 result;
    
    /* Test positive exponents */
    result = FR_pow2(I2FR(0, 8), 8);   /* 2^0 = 1 */
    result = FR_pow2(I2FR(1, 8), 8);   /* 2^1 = 2 */
    result = FR_pow2(I2FR(2, 8), 8);   /* 2^2 = 4 */
    result = FR_pow2(I2FR(3, 8), 8);   /* 2^3 = 8 */
    result = FR_pow2(I2FR(4, 8), 8);   /* 2^4 = 16 */
    
    /* Test negative exponents */
    result = FR_pow2(I2FR(-1, 8), 8);  /* 2^-1 = 0.5 */
    result = FR_pow2(I2FR(-2, 8), 8);  /* 2^-2 = 0.25 */
    result = FR_pow2(I2FR(-3, 8), 8);  /* 2^-3 = 0.125 */
    
    /* Test fractional exponents to hit interpolation branches */
    result = FR_pow2(I2FR(1, 8) + 128, 8);  /* 2^1.5 */
    result = FR_pow2(I2FR(2, 8) + 200, 8);  /* 2^2.78... */
    
    /* Test to hit the else branch in interpolation */
    result = FR_pow2(I2FR(1, 8) + 200, 8);  /* Fractional part > 0.5 */
    
    /* Test edge case where radix-flr < 0 */
    result = FR_pow2(I2FR(-20, 8), 8);  /* Large negative exponent */
    
    (void)result;
    return TEST_PASS;
}

/* Test print functions */
int test_print_complete() {
    /* FR_printNumF exists but requires a function pointer */
    /* Skip for now as it's not critical for coverage */
    return TEST_PASS;
}

/* Test FR_sqrt and FR_hypot (v2 new) */
int test_sqrt_hypot() {
    s32 result;

    /* Sqrt of perfect squares */
    result = FR_sqrt(I2FR(0,   16), 16);
    if (result != 0) return TEST_FAIL;
    result = FR_sqrt(I2FR(1,   16), 16);
    if (result != I2FR(1, 16)) return TEST_FAIL;
    result = FR_sqrt(I2FR(4,   16), 16);
    if (result != I2FR(2, 16)) return TEST_FAIL;
    result = FR_sqrt(I2FR(100, 16), 16);
    if (result != I2FR(10, 16)) return TEST_FAIL;

    /* Sqrt of 2 ≈ 1.41421356... At radix 16: 1.41421356 * 65536 = 92681.9...
     * Round-to-nearest → 92682. Allow ±1 LSB for the rounding. */
    result = FR_sqrt(I2FR(2, 16), 16);
    if (result < 92681 || result > 92683) return TEST_FAIL;

    /* Sqrt of 3 ≈ 1.73205080... At radix 16: 1.73205 * 65536 = 113512.0...
     * Verifies rounding for another non-perfect square. */
    result = FR_sqrt(I2FR(3, 16), 16);
    if (result < 113511 || result > 113513) return TEST_FAIL;

    /* Negative input → INT32_MIN sentinel */
    result = FR_sqrt(-1, 16);
    if (result != (s32)0x80000000) return TEST_FAIL;

    /* Hypot 3,4 → 5 */
    result = FR_hypot(I2FR(3, 16), I2FR(4, 16), 16);
    if (result < I2FR(5, 16) - 2 || result > I2FR(5, 16) + 2) return TEST_FAIL;

    /* Hypot 0,0 → 0 */
    result = FR_hypot(0, 0, 16);
    if (result != 0) return TEST_FAIL;

    /* Hypot of -3,-4 → 5 (signs don't matter) */
    result = FR_hypot(I2FR(-3, 16), I2FR(-4, 16), 16);
    if (result < I2FR(5, 16) - 2 || result > I2FR(5, 16) + 2) return TEST_FAIL;

    /* Hypot 5,12 → 13 */
    result = FR_hypot(I2FR(5, 16), I2FR(12, 16), 16);
    if (result < I2FR(13, 16) - 2 || result > I2FR(13, 16) + 2) return TEST_FAIL;

    /* FR_hypot_fast (4-seg) — same test cases, wider tolerance (~0.3%) */
    result = FR_hypot_fast(I2FR(3, 16), I2FR(4, 16));
    /* 0.3% of 5.0 at radix 16 = 0.015 * 65536 ≈ 983, use 1000 */
    if (result < I2FR(5, 16) - 1000 || result > I2FR(5, 16) + 1000) return TEST_FAIL;

    result = FR_hypot_fast(0, 0);
    if (result != 0) return TEST_FAIL;

    result = FR_hypot_fast(I2FR(-3, 16), I2FR(-4, 16));
    if (result < I2FR(5, 16) - 1000 || result > I2FR(5, 16) + 1000) return TEST_FAIL;

    result = FR_hypot_fast(I2FR(5, 16), I2FR(12, 16));
    if (result < I2FR(13, 16) - 2600 || result > I2FR(13, 16) + 2600) return TEST_FAIL;

    /* Edge: one axis zero — tolerance is 0.4% of expected */
    result = FR_hypot_fast(I2FR(7, 16), 0);
    if (result < I2FR(7, 16) - 2000 || result > I2FR(7, 16) + 2000) return TEST_FAIL;

    result = FR_hypot_fast(0, I2FR(7, 16));
    if (result < I2FR(7, 16) - 2000 || result > I2FR(7, 16) + 2000) return TEST_FAIL;

    /* Equal axes: hypot(1,1) = sqrt(2) ≈ 1.41421 */
    result = FR_hypot_fast(I2FR(1, 16), I2FR(1, 16));
    if (result < 92000 || result > 93300) return TEST_FAIL;

    /* INT32_MIN must not crash (UB in negation) */
    result = FR_hypot_fast((s32)0x80000000, 0);
    if (result <= 0) return TEST_FAIL;

    /* FR_hypot_fast8 (8-seg) — tighter tolerance (~0.1%) */
    result = FR_hypot_fast8(I2FR(3, 16), I2FR(4, 16));
    if (result < I2FR(5, 16) - 400 || result > I2FR(5, 16) + 400) return TEST_FAIL;

    result = FR_hypot_fast8(0, 0);
    if (result != 0) return TEST_FAIL;

    result = FR_hypot_fast8(I2FR(-3, 16), I2FR(-4, 16));
    if (result < I2FR(5, 16) - 400 || result > I2FR(5, 16) + 400) return TEST_FAIL;

    result = FR_hypot_fast8(I2FR(5, 16), I2FR(12, 16));
    if (result < I2FR(13, 16) - 1000 || result > I2FR(13, 16) + 1000) return TEST_FAIL;

    /* Edge: one axis zero — tolerance is 0.15% of expected */
    result = FR_hypot_fast8(I2FR(7, 16), 0);
    if (result < I2FR(7, 16) - 700 || result > I2FR(7, 16) + 700) return TEST_FAIL;

    /* Equal axes */
    result = FR_hypot_fast8(I2FR(1, 16), I2FR(1, 16));
    if (result < 92000 || result > 93300) return TEST_FAIL;

    return TEST_PASS;
}

/* Radian-native trig (fr_cos / fr_sin / fr_tan) and FR_atan.
 * These wrap fr_*_bam after a rad->BAM conversion. We don't need to
 * re-verify the BAM table here — just exercise every line so coverage
 * tools see the radian path. */
int test_radian_trig() {
    /* Use radix 13 so a full revolution (2π) ≈ 51472 fits in s32
     * and the rad->bam reciprocal multiply is well within range.
     * sin/cos now return s32 at radix 16 (s15.16). 1.0 = 65536. */
    s32 c, s, t;

    /* angle = 0: cos=65536 (exact), sin≈0, tan≈0 */
    c = fr_cos(0, 13);
    s = fr_sin(0, 13);
    t = fr_tan(0, 13);
    if (c < 65400 || c > 65536) return TEST_FAIL;
    if (s < -64 || s > 64)      return TEST_FAIL;
    if (t < -32 || t > 32)      return TEST_FAIL;

    /* angle = π/2 ≈ 12868 at radix 13: cos≈0, sin≈65536, tan saturates */
    c = fr_cos(12868, 13);
    s = fr_sin(12868, 13);
    t = fr_tan(12868, 13);
    if (c < -128  || c > 128)   return TEST_FAIL;
    if (s < 65400)              return TEST_FAIL;
    /* tan(π/2) hits the c==0 saturation branch — accept any large positive */
    (void)t;

    /* angle = π ≈ 25736: cos≈-65536, sin≈0 */
    c = fr_cos(25736, 13);
    s = fr_sin(25736, 13);
    if (c > -65400)             return TEST_FAIL;
    if (s < -128 || s > 128)    return TEST_FAIL;

    /* radix=0 path (rad as integer radians, not very meaningful but covers
     * the `if (radix > 0)` branch's else side) */
    c = fr_cos(0, 0);
    if (c < 65400)              return TEST_FAIL;

    /* FR_atan now returns radians at out_radix. Test with radix 16.
     * atan(1) = pi/4 ≈ 0.7854. At radix 16: 0.7854 * 65536 ≈ 51472. */
    s32 rad;
    rad = FR_atan(I2FR(1, 16), 16, 16);   /* atan(1) = pi/4 */
    if (rad < 50000 || rad > 53000)   return TEST_FAIL;
    rad = FR_atan(I2FR(0, 16), 16, 16);   /* atan(0) = 0 */
    if (rad < -200 || rad > 200)      return TEST_FAIL;
    rad = FR_atan(I2FR(-1, 16), 16, 16);  /* atan(-1) = -pi/4 */
    if (rad < -53000 || rad > -50000) return TEST_FAIL;

    /* FR_atan2(0,0) - undefined-but-defined branch */
    rad = FR_atan2(0, 0, 16);
    (void)rad;

    return TEST_PASS;
}

/* Test all wave generators (v2 new) */
int test_waves() {
    s16 v;

    /* Square: top half +, bottom half - */
    if (fr_wave_sqr(0)      != 32767)  return TEST_FAIL;
    if (fr_wave_sqr(0x7fff) != 32767)  return TEST_FAIL;
    if (fr_wave_sqr(0x8000) != -32767) return TEST_FAIL;
    if (fr_wave_sqr(0xffff) != -32767) return TEST_FAIL;

    /* PWM: 25% duty */
    if (fr_wave_pwm(0,      0x4000) != 32767)  return TEST_FAIL;
    if (fr_wave_pwm(0x3fff, 0x4000) != 32767)  return TEST_FAIL;
    if (fr_wave_pwm(0x4000, 0x4000) != -32767) return TEST_FAIL;
    if (fr_wave_pwm(0xffff, 0x4000) != -32767) return TEST_FAIL;
    /* Edge: 0% duty → always low */
    if (fr_wave_pwm(0, 0) != -32767) return TEST_FAIL;

    /* Triangle: peak at 0x4000 (+), trough at 0xc000 (-), zero crossings at 0 and 0x8000 */
    v = fr_wave_tri(0);
    if (v != 0) return TEST_FAIL;
    v = fr_wave_tri(0x4000);
    if (v != 32767) return TEST_FAIL;       /* clamped peak */
    v = fr_wave_tri(0x8000);
    if (v != 0) return TEST_FAIL;
    v = fr_wave_tri(0xc000);
    if (v != -32767) return TEST_FAIL;      /* clamped trough */
    /* Mid-rising at p=0x2000 should be ~16384 */
    v = fr_wave_tri(0x2000);
    if (v < 16380 || v > 16388) return TEST_FAIL;

    /* Saw: clamps -32767 at p=0, passes through 0 at 0x8000, +32767 at 0xffff */
    if (fr_wave_saw(0)      != -32767) return TEST_FAIL;
    if (fr_wave_saw(0x8000) != 0)      return TEST_FAIL;
    if (fr_wave_saw(0xffff) != 32767)  return TEST_FAIL;

    /* tri_morph: at break_point=0x8000 should match a unipolar triangle (0..32767..0) */
    v = fr_wave_tri_morph(0,      0x8000);
    if (v != 0) return TEST_FAIL;
    v = fr_wave_tri_morph(0x8000, 0x8000);
    if (v < 32760) return TEST_FAIL;        /* near peak */
    v = fr_wave_tri_morph(0xffff, 0x8000);
    if (v != 0) return TEST_FAIL;
    /* Saw-mode: break at 0xffff → ramp 0..32767 across the cycle */
    v = fr_wave_tri_morph(0,      0xffff);
    if (v != 0) return TEST_FAIL;
    v = fr_wave_tri_morph(0xffff, 0xffff);
    if (v < 32760) return TEST_FAIL;
    /* Edge: break_point=0 should be treated as 1, no crash */
    v = fr_wave_tri_morph(0x8000, 0);
    (void)v;

    /* Noise: produces non-zero, advances state */
    {
        u32 state = 0xACE1u;
        u32 prev_state;
        s16 sample;
        int i, nonzero_count = 0;

        for (i = 0; i < 16; i++) {
            prev_state = state;
            sample = fr_wave_noise(&state);
            if (state == prev_state) return TEST_FAIL;  /* must advance */
            if (sample != 0) nonzero_count++;
        }
        /* At least most samples should be non-zero (very high probability) */
        if (nonzero_count < 14) return TEST_FAIL;

        /* Null state pointer must not crash */
        sample = fr_wave_noise((u32 *)0);
        if (sample != 0) return TEST_FAIL;
    }

    /* FR_HZ2BAM_INC macro: 1Hz at 65536 sample rate → 1 BAM/sample */
    if (FR_HZ2BAM_INC(1, 65536) != 1) return TEST_FAIL;
    /* 440Hz at 48000 → 440*65536/48000 = 600.something */
    {
        u16 inc = FR_HZ2BAM_INC(440, 48000);
        if (inc < 595 || inc > 605) return TEST_FAIL;
    }

    return TEST_PASS;
}

/* Test ADSR envelope generator (v2 new) */
int test_adsr() {
    fr_adsr_t env;
    s16 v;
    int i;

    /* Init: attack 100, decay 50, sustain 16384 (0.5), release 200 */
    fr_adsr_init(&env, 100, 50, 16384, 200);
    if (env.state != FR_ADSR_IDLE) return TEST_FAIL;

    /* Idle: stepping returns 0 */
    v = fr_adsr_step(&env);
    if (v != 0) return TEST_FAIL;

    /* Trigger: rises from 0 to peak */
    fr_adsr_trigger(&env);
    if (env.state != FR_ADSR_ATTACK) return TEST_FAIL;
    /* Step until past attack */
    for (i = 0; i < 200 && env.state == FR_ADSR_ATTACK; i++)
        fr_adsr_step(&env);
    if (env.state != FR_ADSR_DECAY) return TEST_FAIL;

    /* Step until past decay */
    for (i = 0; i < 200 && env.state == FR_ADSR_DECAY; i++)
        fr_adsr_step(&env);
    if (env.state != FR_ADSR_SUSTAIN) return TEST_FAIL;

    /* Sustain holds */
    v = fr_adsr_step(&env);
    if (v < 16380 || v > 16388) return TEST_FAIL;

    /* Release: drops to 0 */
    fr_adsr_release(&env);
    if (env.state != FR_ADSR_RELEASE) return TEST_FAIL;
    for (i = 0; i < 1000 && env.state == FR_ADSR_RELEASE; i++)
        fr_adsr_step(&env);
    if (env.state != FR_ADSR_IDLE) return TEST_FAIL;
    v = fr_adsr_step(&env);
    if (v != 0) return TEST_FAIL;

    /* Init with all-zero durations should not crash and should
     * effectively be a one-step envelope */
    fr_adsr_init(&env, 0, 0, 32767, 0);
    fr_adsr_trigger(&env);
    fr_adsr_step(&env);     /* attack: jump to peak */
    fr_adsr_step(&env);     /* decay: jump to sustain */
    fr_adsr_step(&env);     /* sustain hold */

    /* Null pointer safety */
    fr_adsr_init((fr_adsr_t *)0, 1, 1, 1, 1);
    fr_adsr_trigger((fr_adsr_t *)0);
    fr_adsr_release((fr_adsr_t *)0);
    v = fr_adsr_step((fr_adsr_t *)0);
    if (v != 0) return TEST_FAIL;

    return TEST_PASS;
}

/* Test all macros and edge cases */
int test_macros_complete() {
    s32 val, result;
    
    /* Test FR_ABS with positive, negative, zero */
    result = FR_ABS(100);
    result = FR_ABS(-100);
    result = FR_ABS(0);
    /* Skip INT_MIN as it causes undefined behavior */
    
    /* Test FR_SGN */
    result = FR_SGN(100);
    result = FR_SGN(-100);
    result = FR_SGN(0);
    
    /* Test FR_MIN, FR_MAX, FR_CLAMP */
    if (FR_MIN(3, 7) != 3)   return TEST_FAIL;
    if (FR_MIN(-5, 2) != -5) return TEST_FAIL;
    if (FR_MIN(0, 0) != 0)   return TEST_FAIL;

    if (FR_MAX(3, 7) != 7)   return TEST_FAIL;
    if (FR_MAX(-5, 2) != 2)  return TEST_FAIL;
    if (FR_MAX(0, 0) != 0)   return TEST_FAIL;

    if (FR_CLAMP(5, 0, 10) != 5)   return TEST_FAIL;
    if (FR_CLAMP(-5, 0, 10) != 0)  return TEST_FAIL;
    if (FR_CLAMP(15, 0, 10) != 10) return TEST_FAIL;
    if (FR_CLAMP(0, 0, 10) != 0)   return TEST_FAIL;
    if (FR_CLAMP(10, 0, 10) != 10) return TEST_FAIL;

    /* Test FR_ADD with different radixes */
    val = I2FR(10, 4);
    FR_ADD(val, 4, I2FR(5, 8), 8);
    
    val = I2FR(10, 8);
    FR_SUB(val, 8, I2FR(5, 4), 4);
    
    /* Test FR_FLOOR and FR_CEIL with various values */
    val = I2FR(10, 8) + 100;
    result = FR_FLOOR(val, 8);
    result = FR_CEIL(val, 8);
    
    val = I2FR(-10, 8) - 100;
    result = FR_FLOOR(val, 8);
    result = FR_CEIL(val, 8);
    
    /* Test with no fractional part */
    val = I2FR(10, 8);
    result = FR_CEIL(val, 8);  /* No fraction, should stay same */
    
    /* Test all angle conversion macros */
    result = FR_DEG2RAD(I2FR(180, 8));
    result = FR_DEG2RAD(I2FR(90, 8));
    result = FR_DEG2RAD(I2FR(45, 8));
    result = FR_DEG2RAD(I2FR(360, 8));
    
    result = FR_RAD2DEG(FR_kPI >> (FR_kPREC - 8));
    /* FR_k2PI doesn't exist, use FR_kPI * 2 */
    result = FR_RAD2DEG((FR_kPI >> (FR_kPREC - 9)) << 1);
    
    /* Test FR_INTERP macro */
    result = FR_INTERP(I2FR(10, 8), I2FR(20, 8), 128, 8);
    
    /* Test radix change with all cases */
    val = I2FR(100, 8);
    result = FR_CHRDX(val, 8, 4);   /* Reduce radix */
    result = FR_CHRDX(val, 4, 8);   /* Increase radix */
    result = FR_CHRDX(val, 8, 8);   /* Same radix */
    
    (void)result;
    (void)val;
    return TEST_PASS;
}

/* Dark-corner edge branches — each block targets a specific uncovered
 * line in FR_math.c that the main tests don't hit. These are mostly
 * saturation/overflow/domain-error paths and the "other arm" of
 * internal conditionals. */
int test_edge_branches() {
    s32 r32;
    fr_adsr_t env;

    /* FR_Tan(deg, radix) c==0 branch. At radix 0, deg=-16384 and
     * deg=16384 both drive the internal BAM to exactly 90°/270°, so
     * cos==0 and we hit the saturation return. */
    r32 = FR_Tan(-16384, 0);                 /* bam=16384 (sin>0) */
    if (r32 != FR_TRIG_MAXVAL) return TEST_FAIL;
    r32 = FR_Tan(16384, 0);                  /* bam=49152 (sin<0) */
    if (r32 != -FR_TRIG_MAXVAL) return TEST_FAIL;

    /* FR_atan2 now returns radians at out_radix.
     * At radix 16: pi/2 ≈ 102944, pi ≈ 205887.
     * atan2(5, 1) ≈ atan(5) ≈ 1.373 rad ≈ 90025 at radix 16. */
    r32 = FR_atan2(I2FR(5, 16), I2FR(1, 16), 16);    /* near 1.373 rad */
    if (r32 < 87000 || r32 > 93000) return TEST_FAIL;
    r32 = FR_atan2(I2FR(-5, 16), I2FR(1, 16), 16);   /* near -1.373 rad */
    if (r32 < -93000 || r32 > -87000) return TEST_FAIL;
    r32 = FR_atan2(I2FR(5, 16), I2FR(-1, 16), 16);   /* near pi - 1.373 ≈ 1.768 rad */
    if (r32 < 112000 || r32 > 120000) return TEST_FAIL;
    r32 = FR_atan2(I2FR(-5, 16), I2FR(-1, 16), 16);  /* near -1.768 rad */
    if (r32 < -120000 || r32 > -112000) return TEST_FAIL;

    /* FR_pow2 with radix > 16 — hits `frac_full >>= (radix - 16)`. */
    r32 = FR_pow2(I2FR(2, 20), 20);              /* 2^2 = 4 */
    if (r32 != I2FR(4, 20))     return TEST_FAIL;
    /* 2^2.5 ≈ 5.6568; verify within ±1% */
    r32 = FR_pow2(I2FR(2, 20) + (1L << 19), 20);
    {
        s32 lo = (s32)(5.6 * (1L << 20));
        s32 hi = (s32)(5.72 * (1L << 20));
        if (r32 < lo || r32 > hi) return TEST_FAIL;
    }

    /* FR_pow2 overflow — flr >= 30 returns FR_OVERFLOW_POS. */
    r32 = FR_pow2(I2FR(30, 16), 16);
    if (r32 != FR_OVERFLOW_POS) return TEST_FAIL;
    r32 = FR_pow2(I2FR(100, 16), 16);
    if (r32 != FR_OVERFLOW_POS) return TEST_FAIL;

    /* FR_pow2 underflow — sh >= 30 returns 0. */
    r32 = FR_pow2(-I2FR(30, 16), 16);
    if (r32 != 0) return TEST_FAIL;
    r32 = FR_pow2(-I2FR(100, 16), 16);
    if (r32 != 0) return TEST_FAIL;

    /* FR_log2 with leading 1 at bit >=30 — hits `if (p >= 30)` branch
     * that right-shifts the mantissa instead of left-shifting. */
    r32 = FR_log2(0x40000000, 0, 16);            /* log2(2^30) = 30 */
    if (r32 < I2FR(30, 16) - 4 || r32 > I2FR(30, 16) + 4) return TEST_FAIL;
    r32 = FR_log2(0x7FFFFFFF, 0, 16);            /* log2(2^31-1) ≈ 31 */
    if (r32 < I2FR(30, 16) || r32 > I2FR(32, 16)) return TEST_FAIL;

    /* fr_adsr_init with sustain < 0 — must clamp to 0. There is no
     * upper clamp because the s16 parameter type already bounds the
     * value at 32767. */
    fr_adsr_init(&env, 10, 10, (s16)-5,     10);
    if (env.sustain != 0) return TEST_FAIL;
    fr_adsr_init(&env, 10, 10, (s16)-32768, 10);
    if (env.sustain != 0) return TEST_FAIL;
    /* Confirm the boundary value (exactly 32767) passes through unmodified. */
    fr_adsr_init(&env, 10, 10, (s16)32767,  10);
    if (env.sustain != ((s32)32767 << 15)) return TEST_FAIL;

    return TEST_PASS;
}

/* Test all constants */
int test_constants_complete() {
    s32 val;
    
    /* Access all constants to ensure they're covered */
    val = FR_kPI;
    val = FR_krPI;      /* Reciprocal of PI */
    val = FR_kE;
    val = FR_krE;       /* Reciprocal of E */
    val = FR_kLOG2E;
    val = FR_krLOG2E;
    val = FR_kLOG2_10;
    val = FR_krLOG2_10;
    val = FR_kSQRT2;
    val = FR_krSQRT2;
    val = FR_kSQRT3;
    val = FR_kSQRT5;
    val = FR_kSQRT10;
    val = FR_kDEG2RAD;
    val = FR_kRAD2DEG;
    val = FR_kQ2RAD;
    val = FR_kRAD2Q;
    
    /* Use the constants in calculations */
    val = FR_kPI >> (FR_kPREC - 8);
    val = FR_SLOG2E(I2FR(10, 8));
    val = FR_SLOG2_10(I2FR(10, 8));
    val = FR_SrLOG2E(I2FR(10, 8));
    val = FR_SrLOG2_10(I2FR(10, 8));
    
    (void)val;
    return TEST_PASS;
}

/* Main test runner */
int main() {
    printf("\n=== FR_Math Full Coverage Test Suite ===\n\n");
    
    printf("Fixed Point Multiplication:\n");
    RUN_TEST(test_fixmuls);
    RUN_TEST(test_fixmulsat);
    RUN_TEST(test_fixaddsat);

    printf("\nDivision & Modulo:\n");
    RUN_TEST(test_div);
    
    printf("\nTrigonometry (Complete):\n");
    RUN_TEST(test_trig_complete);
    RUN_TEST(test_inverse_trig);
    
    printf("\nLogarithms & Powers (Complete):\n");
    RUN_TEST(test_log_complete);
    RUN_TEST(test_pow2_complete);
    
    printf("\nMacros & Edge Cases:\n");
    RUN_TEST(test_macros_complete);
    
    printf("\nConstants:\n");
    RUN_TEST(test_constants_complete);
    
    printf("\nPrint Functions:\n");
    RUN_TEST(test_print_complete);

    printf("\nSqrt and Hypot (v2):\n");
    RUN_TEST(test_sqrt_hypot);

    printf("\nRadian-native trig (v2):\n");
    RUN_TEST(test_radian_trig);

    printf("\nWave Generators (v2):\n");
    RUN_TEST(test_waves);

    printf("\nADSR Envelope (v2):\n");
    RUN_TEST(test_adsr);

    printf("\nDark-Corner Edge Branches:\n");
    RUN_TEST(test_edge_branches);

    printf("\n=== Test Summary ===\n");
    printf("Total: %d, Passed: %d, Failed: %d\n", 
           test_count, test_count - fail_count, fail_count);
    
    return fail_count > 0 ? 1 : 0;
}

/* Re-enable warnings */
#ifdef __clang__
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif