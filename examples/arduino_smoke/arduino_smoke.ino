/*
 * FR_Math Arduino smoke test — compile-only, no hardware required.
 *
 * Exercises every major function group to verify FR_math.c links
 * cleanly on AVR (avr-gcc) and ARM (arduino:samd, etc.).
 *
 * Build:
 *   arduino-cli compile --fqbn arduino:avr:uno examples/arduino_smoke
 */

#include "FR_math.h"

#define R 12  /* radix 12 — fits comfortably in s32 on 8-bit */

/* Force the linker to keep each result so the compiler can't
   optimise the calls away. */
static volatile s32 sink;

void setup() {
    Serial.begin(9600);

    /* --- Conversions & arithmetic --- */
    s32 a = I2FR(3, R);
    s32 b = I2FR(2, R);
    FR_ADD(a, R, b, R);
    sink = a;
    sink = FR_DIV(a, R, b, R);
    sink = FR_DIV_TRUNC(a, R, b, R);
    sink = FR_MOD(a, b);

    /* --- Trig (integer degrees) --- */
    sink = FR_CosI(45);
    sink = FR_SinI(90);

    /* --- Trig (BAM / radian) --- */
    u16 bam = FR_DEG2BAM(60);
    sink = fr_cos_bam(bam);
    sink = fr_sin_bam(bam);
    sink = fr_cos(I2FR(1, R), R);
    sink = fr_sin(I2FR(1, R), R);

    /* --- Inverse trig --- */
    sink = FR_atan2(I2FR(1, R), I2FR(1, R), R);
    sink = FR_acos(1 << (R - 1), R, R);  /* acos(0.5) */

    /* --- Log / exp --- */
    sink = FR_log2(I2FR(100, R), R, R);
    sink = FR_ln(I2FR(10, R), R, R);
    sink = FR_log10(I2FR(100, R), R, R);
    sink = FR_pow2(I2FR(3, R), R);
    sink = FR_EXP(I2FR(1, R), R);
    sink = FR_POW10(I2FR(1, R), R);

    /* --- Shift-only (no multiply) variants --- */
    sink = FR_EXP_FAST(I2FR(1, R), R);
    sink = FR_POW10_FAST(I2FR(1, R), R);

    /* --- Roots --- */
    sink = FR_sqrt(I2FR(2, R), R);
    sink = FR_hypot(I2FR(3, R), I2FR(4, R), R);
    sink = FR_hypot_fast(I2FR(3, R), I2FR(4, R));
    sink = FR_hypot_fast8(I2FR(3, R), I2FR(4, R));

    /* --- Wave generators --- */
    u16 phase = 0;
    sink = fr_wave_sqr(phase);
    sink = fr_wave_pwm(phase, 32768);
    sink = fr_wave_tri(phase);
    sink = fr_wave_saw(phase);
    sink = fr_wave_tri_morph(phase, 32768);
    u32 noise_state = 12345;
    sink = fr_wave_noise(&noise_state);

    /* --- ADSR envelope --- */
    fr_adsr_t env;
    fr_adsr_init(&env, 100, 200, 24576, 400);
    fr_adsr_trigger(&env);
    sink = fr_adsr_step(&env);
    fr_adsr_release(&env);
    sink = fr_adsr_step(&env);

    /* --- Print helpers --- */
    sink = FR_numstr("3.14", R);

    Serial.println(F("FR_Math smoke test: all functions linked OK"));
}

void loop() {
    /* nothing */
}
