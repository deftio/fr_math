# FR_Math Examples

This directory contains examples on a few platforms for seeing how FR_math works.  For embedded platforms use of the built-in printxxx functions can help provide readable output in various radix formats.


## Arduino Examples

Each example is a self-contained `.ino` sketch that prints results to
`Serial` at 9600 baud. Install FR_Math via the Arduino Library Manager
or PlatformIO, then open any example from **File > Examples > FR_Math**.

| Example | What it shows |
|---|---|
| [basic-math](basic-math/basic-math.ino) | Fixed-point conversions, add, sub, mul, div, utility macros |
| [trig-functions](trig-functions/trig-functions.ino) | sin, cos, tan, atan2, BAM angles, degree/radian conversion |
| [wave-generators](wave-generators/wave-generators.ino) | sqr, tri, saw, PWM, noise, ADSR envelope |
| [arduino_smoke](arduino_smoke/arduino_smoke.ino) | Compile-only smoke test — exercises every function group |

## Desktop / POSIX Examples

Each desktop example is self-contained with its own `Makefile` and `README.md`.
Build artifacts stay within the example's directory.

| Example | What it shows |
|---|---|
| [posix-example](posix-example/FR_Math_Example1.cpp) | Comprehensive demo of all library features including 2D transforms |
| [fixed-point-basics](fixed-point-basics/) | Educational walkthrough: radix, conversions, add/sub/mul/div, saturation, formatted output |
| [log-exp-curves](log-exp-curves/) | Sweep log2/ln/log10, pow2/exp/pow10, sqrt with error tables vs IEEE double |
| [waveform-synth](waveform-synth/) | Wave generators + ADSR envelope with ASCII art and CSV output modes |
| [trig-accuracy](trig-accuracy/) | FR_Math vs libfixmath trig accuracy comparison (requires libfixmath source) |

Build all from the repo root:

```bash
make examples        # builds all desktop examples
make run-examples    # builds and runs examples 1-3, plus 4 if libfixmath present
```

Or build any single example from its directory:

```bash
cd examples/fixed-point-basics
make run
```

## Using FR_Math in Arduino

```cpp
#include "FR_math.h"

#define R 12  // radix 12

void setup() {
    Serial.begin(9600);
    s32 c45 = FR_CosI(45);            // cos(45 deg) in s15.16
    s32 root2 = FR_sqrt(I2FR(2, R), R); // sqrt(2) at radix 12
    Serial.println(c45);
    Serial.println(root2);
}

void loop() { }
```

## Using FR_Math in PlatformIO

Add to `platformio.ini`:

```ini
lib_deps = deftio/FR_Math
```

Then `#include "FR_math.h"` as above.
