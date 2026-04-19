/*
 * FR_Math Basic Math Example
 *
 * Demonstrates fixed-point arithmetic: conversions, add, subtract,
 * multiply, divide, and utility macros.
 *
 * Output goes to Serial at 9600 baud.
 *
 * License: BSD-2-Clause
 * https://github.com/deftio/fr_math
 */

#include "FR_math.h"

#define R 12  /* radix 12: s19.12, range ±524287, resolution ~0.00024 */

void setup() {
    Serial.begin(9600);
    while (!Serial) { ; }

    Serial.println(F("=== FR_Math Basic Arithmetic ===\n"));

    /* --- Integer <-> fixed-point conversion --- */
    s32 a = I2FR(100, R);          /* 100.0 */
    s32 b = I2FR(37, R);           /*  37.0 */

    Serial.print(F("a = ")); Serial.println(FR_INT(a, R));
    Serial.print(F("b = ")); Serial.println(FR_INT(b, R));

    /* --- Add / subtract (in-place macros) --- */
    s32 sum = a;
    FR_ADD(sum, R, b, R);          /* sum += b */
    Serial.print(F("a + b = ")); Serial.println(FR_INT(sum, R));

    s32 diff = a;
    FR_SUB(diff, R, b, R);        /* diff -= b */
    Serial.print(F("a - b = ")); Serial.println(FR_INT(diff, R));

    /* --- Multiply (saturating, round-to-nearest) --- */
    s32 prod = FR_FixMuls(a, b);   /* result is at radix 2*R */
    prod >>= R;                     /* shift back to radix R  */
    Serial.print(F("a * b = ")); Serial.println(FR_INT(prod, R));

    /* --- Divide (round-to-nearest) --- */
    s32 quot = FR_DIV(a, R, b, R);
    Serial.print(F("a / b = ")); Serial.println(FR_INT(quot, R));

    /* --- Build a fractional number: 3.14159 --- */
    s32 pi = FR_NUM(3, 14159, 5, R);
    Serial.print(F("pi  ~ ")); Serial.println(FR_INT(pi, R));

    /* --- Parse from string --- */
    s32 val = FR_numstr("-12.75", R);
    Serial.print(F("parsed \"-12.75\" = ")); Serial.println(FR_INT(val, R));

    /* --- Utility macros --- */
    Serial.print(F("abs(-5)    = ")); Serial.println(FR_ABS(-5));
    Serial.print(F("min(3,7)   = ")); Serial.println(FR_MIN(3, 7));
    Serial.print(F("max(3,7)   = ")); Serial.println(FR_MAX(3, 7));
    Serial.print(F("clamp(9,0,5) = ")); Serial.println(FR_CLAMP(9, 0, 5));

    /* --- Change radix --- */
    s32 r12 = I2FR(42, 12);
    s32 r8  = FR_CHRDX(r12, 12, 8);  /* convert from radix 12 to radix 8 */
    Serial.print(F("42 @ radix 12 -> radix 8: ")); Serial.println(FR_INT(r8, 8));

    Serial.println(F("\nDone."));
}

void loop() {
    /* nothing */
}
