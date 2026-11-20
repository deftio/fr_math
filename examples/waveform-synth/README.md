# Waveform Synth

Generates waveforms using FR_Math's wave generators and ADSR envelope,
with both ASCII art visualization and CSV output modes.

## What it demonstrates

| Feature | Functions used |
|---------|--------------|
| Square wave | `fr_wave_sqr` |
| Triangle wave | `fr_wave_tri` |
| Sawtooth wave | `fr_wave_saw` |
| PWM (75% duty) | `fr_wave_pwm` |
| Sine wave | `fr_sin_bam` |
| LFSR noise | `fr_wave_noise` |
| ADSR envelope | `fr_adsr_init`, `fr_adsr_trigger`, `fr_adsr_step`, `fr_adsr_release` |
| Amplitude modulation | Sine wave multiplied by ADSR envelope |

## Building

```bash
make            # compiles waveform_synth
make run        # compiles and runs (ASCII art)
make run-csv    # compiles and runs (CSV output)
make clean      # removes build artifacts
```

Or compile manually:

```bash
cc  -I../../src -Wall -Os -DFR_TRIG_S16_SAFE -c ../../src/FR_math.c -o FR_math.o
g++ -I../../src -Wall -Os -DFR_TRIG_S16_SAFE waveform_synth.cpp FR_math.o -lm -o waveform_synth
```

Sine samples are stored as `s16` via `(s16)(fr_sin_bam(phase) >> 1)`, so
this example compiles with `FR_TRIG_S16_SAFE`. See
[trig-crest](../trig-crest/) for the exact-crest vs s16-safe comparison.

## Running

**ASCII art mode** (default):

```bash
./waveform_synth
```

Renders each waveform as a 64-column x 21-row ASCII plot, plus the
ADSR envelope and a combined sin * ADSR amplitude-modulation demo.

**CSV mode**:

```bash
./waveform_synth --csv
```

Outputs CSV with columns: `sample,sqr,tri,saw,pwm,sin,noise,envelope,combined`.
Suitable for importing into a spreadsheet or plotting tool.

## Expected output (ASCII mode)

```
FR_Math — Waveform Synth Demo  (v...)
  256 samples/cycle, BAM increment = 256

  Square (fr_wave_sqr) (256 samples, showing 64 columns)
   +max |********...                            |
      0 |                                       |
   -max |              ...************************|

  Triangle (fr_wave_tri) ...
  ...
  Sin * ADSR (amplitude modulation) ...

--- end ---
```

## Dependencies

- A C++ compiler (g++ or clang++)
- FR_Math source (`../../src/FR_math.c`, `../../src/FR_math.h`, `../../src/FR_defs.h`)
- Standard C math library (`-lm`)
