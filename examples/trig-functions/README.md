# Trig Functions — Arduino Example

Demonstrates sin, cos, tan, atan2, and angle-unit conversions using
integer-only fixed-point math on Arduino.

## What it demonstrates

| Feature | Functions / macros |
|---------|-------------------|
| Integer-degree trig | `FR_CosI`, `FR_SinI` (s15.16 output) |
| BAM angle conversion | `FR_DEG2BAM`, `fr_cos_bam`, `fr_sin_bam` |
| Radian-native trig | `fr_cos`, `fr_sin`, `fr_tan` (arbitrary input radix) |
| Inverse trig | `FR_atan2`, `FR_acos` |
| Angle conversion | `FR_DEG2RAD` (shift-only, no multiply) |

## Hardware

Any Arduino board with a serial port. Output at 9600 baud.

## Building

**Arduino IDE**: Open `trig-functions.ino` from **File > Examples > FR_Math > trig-functions**.

**Arduino CLI**:

```bash
arduino-cli compile --fqbn arduino:avr:uno examples/trig-functions
arduino-cli upload  --fqbn arduino:avr:uno -p /dev/ttyACM0 examples/trig-functions
arduino-cli monitor -p /dev/ttyACM0 --config baudrate=9600
```

## Expected serial output

```
=== FR_Math Trigonometry ===

Integer-degree API (s15.16 output):
  cos(0) = 65536  sin(0) = 0
  cos(45) = 46341  sin(45) = 46340
  cos(90) = 0  sin(90) = 65536
  ...

BAM API:
  60 deg -> BAM = 10922
  cos_bam(60) = 32768
  sin_bam(60) = 56756

Radian API (radix 12):
  cos(1 rad) = 35419
  sin(1 rad) = 55117
  tan(1 rad) = 101994
  ...

Done.
```
