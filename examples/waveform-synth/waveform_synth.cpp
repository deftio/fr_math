/**
 * waveform_synth.cpp — Waveform generation with ASCII art and CSV output
 *
 * Default (no args):  ASCII art rendering of all waveforms + ADSR demo
 * --csv flag:         CSV output (columns: sample, sqr, tri, saw, pwm, sin, noise, envelope)
 *
 * Build:  make ex_waveform
 * Run:    ./build/ex_waveform           (ASCII art)
 *         ./build/ex_waveform --csv     (CSV output)
 *
 * Copyright (C) 2001-2026 M. A. Chatterjee — zlib license (see FR_math.h)
 */

#include <stdio.h>
#include <string.h>

#include "FR_defs.h"
#include "FR_math.h"

#define NUM_SAMPLES    256
#define ROWS           21
#define COLS           64
#define BAM_INC        (65536 / NUM_SAMPLES)  /* one full cycle in 256 samples */
#define PWM_DUTY       49152                  /* 75% duty = 49152/65536 */

/* Map s16 [-32767, +32767] to row [0, ROWS-1].  Top row = +32767. */
static int val_to_row(s16 v)
{
    int row = (int)(ROWS - 1) - (int)(((long)v + 32767L) * (ROWS - 1) / 65534L);
    if (row < 0) row = 0;
    if (row >= ROWS) row = ROWS - 1;
    return row;
}

/* Subsample: pick COLS points from NUM_SAMPLES evenly */
static int sample_index(int col)
{
    return (col * NUM_SAMPLES) / COLS;
}

/* Print one ASCII waveform */
static void ascii_wave(const char *title, s16 *buf, int n)
{
    printf("\n  %s (%d samples, showing %d columns)\n", title, n, COLS);

    /* Build a character grid */
    char grid[ROWS][COLS + 1];
    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++)
            grid[r][c] = ' ';
        grid[r][COLS] = '\0';
    }

    /* Place asterisks */
    for (int c = 0; c < COLS; c++) {
        int idx = sample_index(c);
        if (idx >= n) idx = n - 1;
        int r = val_to_row(buf[idx]);
        grid[r][c] = '*';
    }

    /* Draw with axis labels */
    for (int r = 0; r < ROWS; r++) {
        const char *label = "";
        if (r == 0)             label = "+max";
        else if (r == ROWS / 2) label = "   0";
        else if (r == ROWS - 1) label = "-max";
        printf("  %5s |%s|\n", label, grid[r]);
    }
}

/* Print ASCII for ADSR envelope (0..32767 unipolar) */
static void ascii_envelope(const char *title, s16 *buf, int n)
{
    printf("\n  %s (%d samples, showing %d columns)\n", title, n, COLS);

    char grid[ROWS][COLS + 1];
    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++)
            grid[r][c] = ' ';
        grid[r][COLS] = '\0';
    }

    for (int c = 0; c < COLS; c++) {
        int idx = sample_index(c);
        if (idx >= n) idx = n - 1;
        /* Envelope is 0..32767; scale to full grid: treat 0 as -32767 for display */
        s16 v = (s16)(buf[idx] * 2 - 32767);
        int r = val_to_row(v);
        grid[r][c] = '*';
    }

    for (int r = 0; r < ROWS; r++) {
        const char *label = "";
        if (r == 0)             label = " 1.0";
        else if (r == ROWS / 2) label = " 0.5";
        else if (r == ROWS - 1) label = " 0.0";
        printf("  %5s |%s|\n", label, grid[r]);
    }
}

/* ================================================================== */
int main(int argc, char *argv[])
{
    int csv_mode = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--csv") == 0) csv_mode = 1;
    }

    /* Generate waveform buffers */
    s16 buf_sqr[NUM_SAMPLES];
    s16 buf_tri[NUM_SAMPLES];
    s16 buf_saw[NUM_SAMPLES];
    s16 buf_pwm[NUM_SAMPLES];
    s16 buf_sin[NUM_SAMPLES];
    s16 buf_noise[NUM_SAMPLES];

    u32 noise_state = 0xDEADBEEF;
    u16 phase = 0;

    for (int i = 0; i < NUM_SAMPLES; i++) {
        buf_sqr[i]   = fr_wave_sqr(phase);
        buf_tri[i]   = fr_wave_tri(phase);
        buf_saw[i]   = fr_wave_saw(phase);
        buf_pwm[i]   = fr_wave_pwm(phase, PWM_DUTY);
        buf_sin[i]   = (s16)(fr_sin_bam(phase) >> 1);  /* s15.16 -> s15 approx */
        buf_noise[i] = fr_wave_noise(&noise_state);
        phase += BAM_INC;
    }

    /* ADSR envelope demo */
    #define ENV_TOTAL 512
    s16 buf_env[ENV_TOTAL];

    fr_adsr_t env;
    fr_adsr_init(&env,
        64,     /* attack samples */
        32,     /* decay samples */
        16384,  /* sustain level s0.15 (50%) */
        64      /* release samples */
    );

    fr_adsr_trigger(&env);
    int release_at = 256;
    for (int i = 0; i < ENV_TOTAL; i++) {
        if (i == release_at)
            fr_adsr_release(&env);
        buf_env[i] = fr_adsr_step(&env);
    }

    /* Combined: sin * envelope (amplitude modulation) */
    #define COMBINED_LEN ENV_TOTAL
    s16 buf_combined[COMBINED_LEN];
    phase = 0;
    for (int i = 0; i < COMBINED_LEN; i++) {
        s32 sin_val = fr_sin_bam(phase);  /* s15.16 */
        s32 env_val = (s32)buf_env[i];    /* 0..32767 (s0.15) */
        /* Multiply: (s15.16 * s0.15) >> 15 = s15.16, then >> 1 for s15 */
        s32 combined = (sin_val * env_val) >> 16;
        if (combined > 32767) combined = 32767;
        if (combined < -32767) combined = -32767;
        buf_combined[i] = (s16)combined;
        phase += BAM_INC;
    }

    if (csv_mode) {
        /* CSV header */
        printf("sample,sqr,tri,saw,pwm,sin,noise,envelope,combined\n");

        int max_len = COMBINED_LEN;
        for (int i = 0; i < max_len; i++) {
            printf("%d", i);
            printf(",%d", i < NUM_SAMPLES ? buf_sqr[i]   : 0);
            printf(",%d", i < NUM_SAMPLES ? buf_tri[i]   : 0);
            printf(",%d", i < NUM_SAMPLES ? buf_saw[i]   : 0);
            printf(",%d", i < NUM_SAMPLES ? buf_pwm[i]   : 0);
            printf(",%d", i < NUM_SAMPLES ? buf_sin[i]   : 0);
            printf(",%d", i < NUM_SAMPLES ? buf_noise[i] : 0);
            printf(",%d", buf_env[i]);
            printf(",%d", buf_combined[i]);
            printf("\n");
        }
    } else {
        printf("FR_Math — Waveform Synth Demo  (v%s)\n", FR_MATH_VERSION);
        printf("  %d samples/cycle, BAM increment = %d\n", NUM_SAMPLES, BAM_INC);

        ascii_wave("Square (fr_wave_sqr)",       buf_sqr,  NUM_SAMPLES);
        ascii_wave("Triangle (fr_wave_tri)",     buf_tri,  NUM_SAMPLES);
        ascii_wave("Sawtooth (fr_wave_saw)",     buf_saw,  NUM_SAMPLES);
        ascii_wave("PWM 75%% (fr_wave_pwm)",     buf_pwm,  NUM_SAMPLES);
        ascii_wave("Sine (fr_sin_bam)",          buf_sin,  NUM_SAMPLES);
        ascii_wave("Noise (fr_wave_noise)",      buf_noise, NUM_SAMPLES);

        printf("\n  ADSR params: attack=64, decay=32, sustain=50%%, release=64\n");
        printf("  Trigger at sample 0, release at sample %d, total %d samples\n",
               release_at, ENV_TOTAL);
        ascii_envelope("ADSR Envelope (fr_adsr)", buf_env, ENV_TOTAL);

        ascii_wave("Sin * ADSR (amplitude modulation)", buf_combined, COMBINED_LEN);

        printf("\n  Tip: run with --csv to get machine-readable output\n");
    }

    printf("\n--- end ---\n");
    return 0;
}
