# Arduino Smoke Test

Compile-only smoke test that exercises every major FR_Math function group
to verify that `FR_math.c` links cleanly on AVR (avr-gcc) and ARM targets.
No specific hardware required beyond a board that compiles.

## What it tests

- Conversions and arithmetic: `I2FR`, `FR_ADD`, `FR_DIV`, `FR_DIV_TRUNC`, `FR_MOD`
- Integer-degree trig: `FR_CosI`, `FR_SinI`
- BAM and radian trig: `fr_cos_bam`, `fr_sin_bam`, `fr_cos`, `fr_sin`
- Inverse trig: `FR_atan2`, `FR_acos`
- Log / exp: `FR_log2`, `FR_ln`, `FR_log10`, `FR_pow2`, `FR_EXP`, `FR_POW10`
- Shift-only variants: `FR_EXP_FAST`, `FR_POW10_FAST`
- Roots: `FR_sqrt`, `FR_hypot`, `FR_hypot_fast`, `FR_hypot_fast8`
- Wave generators: `fr_wave_sqr`, `fr_wave_pwm`, `fr_wave_tri`, `fr_wave_saw`, `fr_wave_tri_morph`, `fr_wave_noise`
- ADSR envelope: `fr_adsr_init`, `fr_adsr_trigger`, `fr_adsr_step`, `fr_adsr_release`
- String parsing: `FR_numstr`

## Building

**Arduino CLI** (no upload needed — compile-only test):

```bash
arduino-cli compile --fqbn arduino:avr:uno examples/arduino_smoke
```

If it compiles without errors, all function groups link correctly.

## Expected serial output

If uploaded and run:

```
FR_Math smoke test: all functions linked OK
```
