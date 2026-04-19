/*
 * FR_Math Trigonometry Example
 *
 * Demonstrates sin, cos, tan, atan2, and angle-unit conversions
 * using integer-only fixed-point math.
 *
 * Output goes to Serial at 9600 baud.
 *
 * License: BSD-2-Clause
 * https://github.com/deftio/fr_math
 */

#include "FR_math.h"

#define R 12  /* radix 12: s19.12 */

void setup() {
    Serial.begin(9600);
    while (!Serial) { ; }

    Serial.println(F("=== FR_Math Trigonometry ===\n"));

    /* --- Integer-degree shortcuts (output at s15.16) --- */
    Serial.println(F("Integer-degree API (s15.16 output):"));
    for (int deg = 0; deg <= 360; deg += 45) {
        s32 c = FR_CosI(deg);
        s32 s = FR_SinI(deg);
        Serial.print(F("  cos(")); Serial.print(deg);
        Serial.print(F(") = ")); Serial.print(c);
        Serial.print(F("  sin(")); Serial.print(deg);
        Serial.print(F(") = ")); Serial.println(s);
    }

    /* --- BAM (Binary Angle Measurement) API --- */
    Serial.println(F("\nBAM API:"));
    u16 bam60 = FR_DEG2BAM(60);
    Serial.print(F("  60 deg -> BAM = ")); Serial.println(bam60);
    Serial.print(F("  cos_bam(60) = ")); Serial.println(fr_cos_bam(bam60));
    Serial.print(F("  sin_bam(60) = ")); Serial.println(fr_sin_bam(bam60));

    /* --- Radian API at arbitrary radix --- */
    Serial.println(F("\nRadian API (radix 12):"));
    s32 one_rad = I2FR(1, R);     /* 1.0 radian */
    Serial.print(F("  cos(1 rad) = ")); Serial.println(fr_cos(one_rad, R));
    Serial.print(F("  sin(1 rad) = ")); Serial.println(fr_sin(one_rad, R));
    Serial.print(F("  tan(1 rad) = ")); Serial.println(fr_tan(one_rad, R));

    /* --- Inverse trig --- */
    Serial.println(F("\nInverse trig:"));
    s32 atanval = FR_atan2(I2FR(1, R), I2FR(1, R), R);
    Serial.print(F("  atan2(1,1)  = ")); Serial.print(atanval);
    Serial.println(F("  (should be ~pi/4)"));

    s32 acosval = FR_acos(1 << (R - 1), R, R);  /* acos(0.5) */
    Serial.print(F("  acos(0.5)   = ")); Serial.print(acosval);
    Serial.println(F("  (should be ~pi/3)"));

    /* --- Angle conversions --- */
    Serial.println(F("\nAngle conversions (shift-only):"));
    s32 deg90 = I2FR(90, R);
    s32 rad   = FR_DEG2RAD(deg90);
    Serial.print(F("  90 deg -> rad ~ ")); Serial.println(rad);

    Serial.println(F("\nDone."));
}

void loop() {
    /* nothing */
}
