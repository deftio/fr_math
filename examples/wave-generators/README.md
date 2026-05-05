# Wave Generators — Arduino Example

Demonstrates square, triangle, sawtooth, PWM, morphing triangle,
noise, and ADSR envelope generators on Arduino — all integer-only.

## What it demonstrates

| Feature | Functions / macros |
|---------|-------------------|
| Phase increment | `FR_HZ2BAM_INC` (Hz + sample rate to BAM step) |
| Square wave | `fr_wave_sqr` |
| Triangle wave | `fr_wave_tri` |
| Sawtooth wave | `fr_wave_saw` |
| PWM (variable duty) | `fr_wave_pwm` |
| Morphing triangle | `fr_wave_tri_morph` |
| LFSR noise | `fr_wave_noise` |
| ADSR envelope | `fr_adsr_init`, `fr_adsr_trigger`, `fr_adsr_step`, `fr_adsr_release` |

All wave functions take a `u16` BAM phase and return `s16` in [-32767, +32767].
The ADSR envelope returns `s16` in [0, 32767] (unipolar).

## Hardware

Any Arduino board with a serial port. Output at 9600 baud.

## Building

**Arduino IDE**: Open `wave-generators.ino` from **File > Examples > FR_Math > wave-generators**.

**Arduino CLI**:

```bash
arduino-cli compile --fqbn arduino:avr:uno examples/wave-generators
arduino-cli upload  --fqbn arduino:avr:uno -p /dev/ttyACM0 examples/wave-generators
arduino-cli monitor -p /dev/ttyACM0 --config baudrate=9600
```

## Expected serial output

```
=== FR_Math Wave Generators ===

440 Hz @ 8 kHz -> phase_inc = 3604

phase   sqr     tri     saw     pwm75   morph
0       32767   0       -32767  32767   0
4096    32767   8192    -24575  32767   32767
...

Noise (10 samples):
16214 -8277 32725 ...

ADSR envelope (attack=100, decay=200, sustain=0.75, release=400):
327 654 981 ...

Done.
```
