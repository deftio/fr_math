/*
 * FR_Math Wave Generators Example
 *
 * Demonstrates square, triangle, sawtooth, PWM, morphing triangle,
 * noise, and ADSR envelope generators — all integer-only.
 *
 * Output goes to Serial at 9600 baud.
 *
 * License: BSD-2-Clause
 * https://github.com/deftio/fr_math
 */

#include "FR_math.h"

#define SAMPLE_RATE 8000
#define FREQ_HZ     440

void setup() {
    Serial.begin(9600);
    while (!Serial) { ; }

    Serial.println(F("=== FR_Math Wave Generators ===\n"));

    /* Phase increment for 440 Hz at 8000 Hz sample rate.
     * Phase is a u16 that wraps at 65536 (BAM). */
    u16 phase_inc = FR_HZ2BAM_INC(FREQ_HZ, SAMPLE_RATE);
    Serial.print(F("440 Hz @ 8 kHz -> phase_inc = "));
    Serial.println(phase_inc);

    /* Print one cycle of each waveform (16 samples). */
    u16 phase = 0;
    u16 step  = 65536U / 16;  /* 16 samples per cycle */

    Serial.println(F("\nphase\tsqr\ttri\tsaw\tpwm75\tmorph"));
    for (int i = 0; i < 16; i++) {
        s16 sqr   = fr_wave_sqr(phase);
        s16 tri   = fr_wave_tri(phase);
        s16 saw   = fr_wave_saw(phase);
        s16 pwm75 = fr_wave_pwm(phase, 49152);    /* 75% duty */
        s16 morph = fr_wave_tri_morph(phase, 16384); /* 25% break */

        Serial.print(phase); Serial.print('\t');
        Serial.print(sqr);   Serial.print('\t');
        Serial.print(tri);   Serial.print('\t');
        Serial.print(saw);   Serial.print('\t');
        Serial.print(pwm75); Serial.print('\t');
        Serial.println(morph);

        phase += step;
    }

    /* --- Noise generator --- */
    Serial.println(F("\nNoise (10 samples):"));
    u32 noise_state = 12345;
    for (int i = 0; i < 10; i++) {
        s16 n = fr_wave_noise(&noise_state);
        Serial.print(n); Serial.print(' ');
    }
    Serial.println();

    /* --- ADSR envelope --- */
    Serial.println(F("\nADSR envelope (attack=100, decay=200, sustain=0.75, release=400):"));
    fr_adsr_t env;
    fr_adsr_init(&env, 100, 200, 24576, 400);  /* sustain=24576/32768 ~ 0.75 */
    fr_adsr_trigger(&env);

    for (int i = 0; i < 20; i++) {
        s16 val = fr_adsr_step(&env);
        Serial.print(val); Serial.print(' ');
        if (i == 14) {
            fr_adsr_release(&env);  /* trigger release at step 15 */
            Serial.print(F("[rel] "));
        }
    }
    Serial.println();

    Serial.println(F("\nDone."));
}

void loop() {
    /* nothing */
}
