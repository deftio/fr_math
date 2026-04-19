# FR_Math Examples

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

## Desktop / POSIX Example

| Example | What it shows |
|---|---|
| [posix-example](posix-example/FR_Math_Example1.cpp) | Comprehensive demo of all library features including 2D transforms (requires `<stdio.h>`, `<math.h>`) |

Build the POSIX example with:

```bash
make examples        # produces build/fr_example
./build/fr_example
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
