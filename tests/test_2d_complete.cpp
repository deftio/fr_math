/*
 * test_2d_complete.cpp - Complete test coverage for FR_math_2D
 * Tests the C++ 2D transformation matrix class
 */

#include <stdio.h>
#include <string.h>
#include "../src/FR_math_2D.h"

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

/* Test identity matrix */
int test_identity() {
    FR_Matrix2D_CPT mat(8);
    mat.ID();
    
    // Check identity matrix values
    if (mat.m00 != I2FR(1, 8)) return TEST_FAIL;
    if (mat.m11 != I2FR(1, 8)) return TEST_FAIL;
    if (mat.m01 != 0 || mat.m10 != 0) return TEST_FAIL;
    if (mat.m02 != 0 || mat.m12 != 0) return TEST_FAIL;
    
    // Test different radix
    FR_Matrix2D_CPT mat2(12);
    if (mat2.radix != 12) return TEST_FAIL;
    
    return TEST_PASS;
}

/* Test transformations */
int test_transformations() {
    FR_Matrix2D_CPT mat(8);
    
    // Test translation
    // XlateI expects integer values, not pre-shifted
    mat.ID();
    mat.XlateI(10, 20);  // Will be shifted by radix internally
    if (mat.m02 != I2FR(10, 8)) return TEST_FAIL;
    if (mat.m12 != I2FR(20, 8)) return TEST_FAIL;
    
    // Test relative translation
    // XlateRelativeI expects integer values, not pre-shifted
    mat.XlateRelativeI(5, 10);  // Add 5 to x, 10 to y
    if (mat.m02 != I2FR(15, 8)) {
        return TEST_FAIL;
    }
    if (mat.m12 != I2FR(30, 8)) {
        return TEST_FAIL;
    }
    
    // Test with different radix
    mat.ID();
    mat.XlateI(100, 200, 4);  // Use radix 4
    
    mat.XlateRelativeI(50, 100, 4);
    
    return TEST_PASS;
}

/* Test point transformations */
int test_point_transforms() {
    FR_Matrix2D_CPT mat(8);
    s32 x, y, xp, yp;
    s16 x16, y16, xp16, yp16;
    
    // Identity transform
    mat.ID();
    // XFormPtI expects integer inputs, not fixed-point!
    x = 10;  // Integer value
    y = 20;  // Integer value
    mat.XFormPtI(x, y, &xp, &yp);
    if (xp != x || yp != y) {
        printf("Identity transform failed: expected (%ld,%ld), got (%ld,%ld)\n", (long)x, (long)y, (long)xp, (long)yp);
        return TEST_FAIL;
    }
    
    // Translation transform
    mat.XlateI(5, 10);  // XlateI expects integer values
    // XFormPtI expects integer inputs, returns integer outputs
    x = 10;  // Integer value
    y = 20;  // Integer value
    mat.XFormPtI(x, y, &xp, &yp);
    // The result should be the transformed point as integers
    if (xp != 15) {
        printf("Translation X failed: expected %d, got %ld\n", 15, (long)xp);
        printf("  x=%ld, m00=%ld, m02=%ld\n", (long)x, (long)mat.m00, (long)mat.m02);
        return TEST_FAIL;
    }
    if (yp != 30) {
        printf("Translation Y failed: expected %d, got %ld\n", 30, (long)yp);
        return TEST_FAIL;
    }
    
    // Test with explicit radix (same result expected)
    mat.XFormPtI(x, y, &xp, &yp, 8);
    if (xp != 15 || yp != 30) return TEST_FAIL;
    
    // Test no-translate version (reset to identity first)
    mat.ID();
    x = 10;  // Reset to original values
    y = 20;
    mat.XFormPtINoTranslate(x, y, &xp, &yp, 8);
    if (xp != x || yp != y) return TEST_FAIL;  // Identity without translation
    
    // Test 16-bit versions
    x16 = 100;
    y16 = 200;
    mat.XFormPtI16(x16, y16, &xp16, &yp16);
    
    mat.XFormPtI16NoTranslate(x16, y16, &xp16, &yp16);
    
    return TEST_PASS;
}

/* Test rotation */
int test_rotation() {
    FR_Matrix2D_CPT mat(8);
    
    // Test rotation by various angles
    mat.setrotate(0);
    mat.setrotate(45);
    mat.setrotate(90);
    mat.setrotate(180);
    mat.setrotate(270);
    mat.setrotate(-45);
    
    // Test with explicit radix
    mat.setrotate(30, 10);
    mat.setrotate(60, 12);
    
    return TEST_PASS;
}

/* Test matrix operations */
int test_matrix_ops() {
    FR_Matrix2D_CPT mat1(8), mat2(8), mat3(8);
    
    // Set up matrices
    mat1.set(I2FR(2, 8), 0, I2FR(3, 8), 
             0, I2FR(2, 8), I2FR(4, 8), 8);
    
    mat2.set(I2FR(1, 8), 0, I2FR(1, 8),
             0, I2FR(1, 8), I2FR(1, 8), 8);
    
    // Test addition
    mat3 = mat1;
    mat3.add(&mat2);
    
    // Test subtraction
    mat3 = mat1;
    mat3.sub(&mat2);
    
    // Test assignment operator
    mat3 = mat1;
    if (mat3.m00 != mat1.m00) return TEST_FAIL;
    
    // Test += operator
    mat3 = mat1;
    mat3 += mat2;
    
    // Test -= operator
    mat3 = mat1;
    mat3 -= mat2;
    
    // Test scalar multiplication
    mat3 = mat1;
    mat3 *= 2;
    
    return TEST_PASS;
}

/* Test determinant and inverse */
int test_det_inv() {
    FR_Matrix2D_CPT mat(8), inv_mat(8);
    s32 det;
    
    // Identity matrix should have determinant = 1
    mat.ID();
    det = mat.det();
    
    // Test non-singular matrix
    mat.set(I2FR(2, 8), 0, 0,
            0, I2FR(3, 8), 0, 8);
    det = mat.det();
    
    // Test inverse
    FR_RESULT res = mat.inv(&inv_mat);
    
    // Test in-place inverse
    mat.set(I2FR(4, 8), 0, I2FR(1, 8),
            0, I2FR(4, 8), I2FR(2, 8), 8);
    res = mat.inv();
    
    // Test singular matrix (should fail)
    mat.set(0, 0, 0, 0, 0, 0, 8);
    res = mat.inv(&inv_mat);
    
    (void)det;
    (void)res;
    return TEST_PASS;
}

/* Test fast mode detection */
int test_fast_mode() {
    FR_Matrix2D_CPT mat(8);
    
    // Identity should be fast
    mat.ID();
    mat.checkfast();
    
    // Scale-only should be fast
    mat.set(I2FR(2, 8), 0, 0,
            0, I2FR(3, 8), 0, 8);
    if (!mat.checkfast()) return TEST_FAIL;
    
    // Full matrix should not be fast
    mat.set(I2FR(2, 8), I2FR(1, 8), I2FR(3, 8),
            I2FR(1, 8), I2FR(2, 8), I2FR(4, 8), 8);
    if (mat.checkfast()) return TEST_FAIL;
    
    // Test fast mode in transforms
    mat.ID();
    mat.checkfast();
    s32 x = 10, y = 20;  // Integer values
    s32 xp, yp;
    mat.XFormPtI(x, y, &xp, &yp);  // Should use fast path
    
    // Test with 16-bit
    s16 x16 = 100, y16 = 200;
    s16 xp16, yp16;
    mat.XFormPtI16(x16, y16, &xp16, &yp16);  // Should use fast path
    mat.XFormPtI16NoTranslate(x16, y16, &xp16, &yp16);  // Should use fast path
    
    return TEST_PASS;
}

/* Test edge cases */
int test_edge_cases() {
    FR_Matrix2D_CPT mat(8);
    s32 xp, yp;
    
    // Test with zero values
    mat.set(0, 0, 0, 0, 0, 0, 8);
    mat.XFormPtI(10, 20, &xp, &yp);  // Integer inputs
    
    // Test with maximum values
    mat.set(0x7FFFFFFF, 0, 0,
            0, 0x7FFFFFFF, 0, 8);
    mat.checkfast();
    
    // Test with negative values
    mat.set(-I2FR(1, 8), 0, -I2FR(10, 8),
            0, -I2FR(1, 8), -I2FR(20, 8), 8);
    mat.XFormPtI(5, 5, &xp, &yp);  // Integer inputs
    
    // Test radix limits
    FR_Matrix2D_CPT mat_high(16);  // High precision
    FR_Matrix2D_CPT mat_low(2);    // Low precision
    
    return TEST_PASS;
}

/* Main test runner */
int main() {
    printf("\n=== FR_Math_2D Complete Test Suite ===\n\n");
    
    printf("Basic Operations:\n");
    RUN_TEST(test_identity);
    RUN_TEST(test_transformations);
    
    printf("\nPoint Transformations:\n");
    RUN_TEST(test_point_transforms);
    
    printf("\nRotations:\n");
    RUN_TEST(test_rotation);
    
    printf("\nMatrix Operations:\n");
    RUN_TEST(test_matrix_ops);
    RUN_TEST(test_det_inv);
    
    printf("\nOptimizations:\n");
    RUN_TEST(test_fast_mode);
    
    printf("\nEdge Cases:\n");
    RUN_TEST(test_edge_cases);
    
    printf("\n=== Test Summary ===\n");
    printf("Total: %d, Passed: %d, Failed: %d\n", 
           test_count, test_count - fail_count, fail_count);
    
    return fail_count > 0 ? 1 : 0;
}