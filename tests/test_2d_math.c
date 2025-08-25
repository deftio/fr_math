/*
 * test_2d_math.c - Basic smoke test for 2D math compilation
 * The actual 2D math testing is done in test_2d_complete.cpp
 * since FR_math_2D is a C++ API
 */

#include <stdio.h>
#include "../src/FR_math.h"

int main() {
    printf("\n=== FR_Math_2D Basic Compilation Test ===\n");
    printf("Note: Full 2D math testing is in test_2d_complete.cpp\n");
    printf("This test just verifies the library links correctly.\n\n");
    
    // Just test that basic FR_math functions work
    s32 val = I2FR(10, 8);
    s32 result = FR_CosI(45);
    
    printf("Basic math test: cos(45) = %d (fixed point)\n", result);
    printf("Test passed.\n");
    
    return 0;
}