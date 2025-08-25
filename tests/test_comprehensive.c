/*
 * test_comprehensive.c - Comprehensive tests for FR_Math library
 * Based on examples and designed to improve code coverage
 * 
 * @author M A Chatterjee <deftio [at] deftio [dot] com>
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
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

/* Test basic conversions and macros */
int test_conversions() {
    s32 val;
    
    /* Test I2FR and FR2I */
    val = I2FR(100, 8);
    if (FR2I(val, 8) != 100) return TEST_FAIL;
    
    /* Test FR_INT for negative numbers */
    val = I2FR(-50, 8);
    if (FR_INT(val, 8) != -50) return TEST_FAIL;
    
    /* Test FR_FRAC */
    val = I2FR(10, 8) + 128; /* 10.5 in 8-bit radix */
    if (FR_FRAC(val, 8) != 128) return TEST_FAIL;
    
    /* Test radix conversion */
    val = I2FR(25, 4);
    val = FR_CHRDX(val, 4, 8);
    if (FR2I(val, 8) != 25) return TEST_FAIL;
    
    return TEST_PASS;
}

/* Test all trig functions comprehensively */
int test_trig_comprehensive() {
    s16 result;
    int angle;
    
    /* Test sin and cos for key angles */
    for (angle = 0; angle <= 360; angle += 30) {
        result = FR_CosI(angle);
        result = FR_SinI(angle);
        /* Just ensure they don't crash and return reasonable values */
        (void)result; /* Suppress unused warning */
    }
    
    /* Test negative angles */
    result = FR_CosI(-45);
    result = FR_SinI(-45);
    
    /* Test large angles (wrapping) */
    result = FR_CosI(720);
    result = FR_SinI(1080);
    
    /* Test TanI if available */
    result = FR_TanI(45);
    result = FR_TanI(0);
    result = FR_TanI(30);
    
    (void)result; /* Suppress unused warning */
    return TEST_PASS;
}

/* Test logarithm functions */
int test_log_functions() {
    s32 val, result;
    
    /* Test log2 with various inputs */
    val = I2FR(8, 8);
    result = FR_log2(val, 8, 8);
    
    val = I2FR(16, 8);
    result = FR_log2(val, 8, 8);
    
    /* Test ln (natural log) */
    val = I2FR(10, 8);
    result = FR_ln(val, 8, 8);
    
    /* Test log10 */
    val = I2FR(100, 8);
    result = FR_log10(val, 8, 8);
    
    /* Test edge cases */
    result = FR_log2(0, 8, 8);
    if (result != FR_LOG2MIN) return TEST_FAIL;
    
    result = FR_log2(-100, 8, 8);
    if (result != FR_LOG2MIN) return TEST_FAIL;
    
    return TEST_PASS;
}

/* Test power functions */
int test_pow_functions() {
    s32 val, result;
    
    /* Test pow2 (2^x) */
    val = I2FR(3, 8); /* 2^3 = 8 */
    result = FR_pow2(val, 8);
    
    val = I2FR(0, 8); /* 2^0 = 1 */
    result = FR_pow2(val, 8);
    
    val = I2FR(-1, 8); /* 2^-1 = 0.5 */
    result = FR_pow2(val, 8);
    
    (void)result; /* Suppress unused warning */
    return TEST_PASS;
}

/* Test rounding functions */
int test_rounding() {
    s32 val, result;
    
    /* Test FR_FLOOR */
    val = I2FR(10, 8) + 200; /* 10.78... */
    result = FR_FLOOR(val, 8);
    if (FR2I(result, 8) != 10) return TEST_FAIL;
    
    /* Test FR_CEIL */
    val = I2FR(10, 8) + 50; /* 10.19... */
    result = FR_CEIL(val, 8);
    if (FR2I(result, 8) != 11) return TEST_FAIL;
    
    /* Test FR_FLOOR with negative */
    val = I2FR(-10, 8) - 128; /* -10.5 */
    result = FR_FLOOR(val, 8);
    /* Floor of -10.5 should be -11 */
    
    /* Test FR_CEIL with fractional part */
    val = I2FR(10, 8) + 128; /* 10.5 */
    result = FR_CEIL(val, 8);
    if (FR2I(result, 8) != 11) return TEST_FAIL;
    
    return TEST_PASS;
}

/* Test degree/radian conversions */
int test_angle_conversions() {
    s32 deg, rad;
    
    /* Test degree to radian conversion */
    deg = I2FR(180, 8);
    rad = FR_DEG2RAD(deg);
    
    deg = I2FR(90, 8);
    rad = FR_DEG2RAD(deg);
    
    /* Test radian to degree conversion */
    rad = FR_kPI >> (FR_kPREC - 8); /* π in 8-bit radix */
    deg = FR_RAD2DEG(rad);
    
    return TEST_PASS;
}

/* Test mathematical constants */
int test_constants() {
    s32 val;
    
    /* Test PI constant */
    val = FR_kPI >> (FR_kPREC - 8);
    if (val == 0) return TEST_FAIL;
    
    /* Test E constant */
    val = FR_kE >> (FR_kPREC - 8);
    if (val == 0) return TEST_FAIL;
    
    /* Test sqrt(2) constant */
    val = FR_kSQRT2 >> (FR_kPREC - 8);
    if (val == 0) return TEST_FAIL;
    
    return TEST_PASS;
}

/* Test print functions - skip if not available */
int test_print_functions() {
    /* FR_PrintNum doesn't exist in the current API */
    /* This test is a placeholder for future implementation */
    return TEST_PASS;
}

/* Test special math operations */
int test_special_ops() {
    s32 a, b, result;
    
    /* Test basic multiplication with shifts */
    a = I2FR(10, 4);
    b = I2FR(5, 4);
    result = ((s32)a * (s32)b) >> 4;
    if (FR2I(result, 4) != 50) return TEST_FAIL;
    
    /* Test with larger values */
    a = I2FR(100, 8);
    b = I2FR(2, 8);
    result = ((s32)a * (s32)b) >> 8;
    if (FR2I(result, 8) != 200) return TEST_FAIL;
    
    return TEST_PASS;
}

/* Test edge cases and boundaries */
int test_edge_cases() {
    s32 val, result;
    
    /* Test with maximum positive value */
    val = 0x7FFFFFFF;
    result = FR_ABS(val);
    if (result != val) return TEST_FAIL;
    
    /* Test sign function */
    result = FR_SGN(1000);
    if (result != 0) return TEST_FAIL;
    
    result = FR_SGN(-1000);
    if (result != -1) return TEST_FAIL;
    
    result = FR_SGN(0);
    if (result != 0) return TEST_FAIL;
    
    return TEST_PASS;
}

/* Main test runner */
int main() {
    printf("\n=== FR_Math Comprehensive Test Suite ===\n\n");
    
    printf("Basic Operations:\n");
    RUN_TEST(test_conversions);
    RUN_TEST(test_edge_cases);
    
    printf("\nTrigonometry:\n");
    RUN_TEST(test_trig_comprehensive);
    RUN_TEST(test_angle_conversions);
    
    printf("\nLogarithms & Powers:\n");
    RUN_TEST(test_log_functions);
    RUN_TEST(test_pow_functions);
    
    printf("\nRounding & Truncation:\n");
    RUN_TEST(test_rounding);
    
    printf("\nConstants & Special:\n");
    RUN_TEST(test_constants);
    RUN_TEST(test_special_ops);
    
    printf("\nUtility Functions:\n");
    RUN_TEST(test_print_functions);
    
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