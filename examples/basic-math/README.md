# Basic Math — Arduino Example

Demonstrates fixed-point arithmetic fundamentals on Arduino:
conversions, add, subtract, multiply, divide, and utility macros.

## What it demonstrates

| Operation | Functions / macros |
|-----------|-------------------|
| Integer to fixed-point | `I2FR`, `FR_INT` |
| Addition / subtraction | `FR_ADD`, `FR_SUB` |
| Multiplication | `FR_FixMuls` (round-to-nearest) |
| Division | `FR_DIV` (64-bit, rounded) |
| Constant construction | `FR_NUM` (build 3.14159 from parts) |
| String parsing | `FR_numstr` |
| Utility macros | `FR_ABS`, `FR_MIN`, `FR_MAX`, `FR_CLAMP` |
| Radix change | `FR_CHRDX` |

## Hardware

Any Arduino board with a serial port. Output at 9600 baud.

## Building

**Arduino IDE**: Open `basic-math.ino` from **File > Examples > FR_Math > basic-math**.

**Arduino CLI**:

```bash
arduino-cli compile --fqbn arduino:avr:uno examples/basic-math
arduino-cli upload  --fqbn arduino:avr:uno -p /dev/ttyACM0 examples/basic-math
arduino-cli monitor -p /dev/ttyACM0 --config baudrate=9600
```

**PlatformIO**:

```bash
pio run -e uno
pio run -e uno -t upload
pio device monitor -b 9600
```

## Expected serial output

```
=== FR_Math Basic Arithmetic ===

a = 100
b = 37
a + b = 137
a - b = 63
a * b = 3700
a / b = 2
pi  ~ 3
parsed "-12.75" = -12
abs(-5)    = 5
min(3,7)   = 3
max(3,7)   = 7
clamp(9,0,5) = 5
42 @ radix 12 -> radix 8: 42

Done.
```
