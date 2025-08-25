# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview

FR_Math is a C language fixed-point math library for embedded systems. It provides integer-based math operations using fixed radix (binary point) representations, allowing fractional calculations without floating-point support.

## Building and Testing

### Build Commands

```bash
# Build example program
make FR_math_example1.exe

# Build and run tests with coverage
make FR_math_test.exe && ./FR_math_test.exe

# Clean build artifacts
make clean

# Clean all files including backups
make cleanall
```

### Compilation Flags
The library uses `-Wall -Os -ftest-coverage -fprofile-arcs` for testing builds with coverage support.

## Architecture

### Core Components

1. **FR_defs.h** - Type definitions and platform abstractions
   - Defines `s8`, `s16`, `s32` for signed integers
   - Defines `u8`, `u16`, `u32` for unsigned integers

2. **FR_math.h/c** - Core fixed-radix math operations
   - Macros for basic operations (add, multiply, conversions)
   - Trigonometric functions (sin, cos, tan)
   - Logarithmic and exponential functions
   - Fixed-point to integer conversions

3. **FR_math_2D.h/cpp** - 2D coordinate transformations
   - Matrix operations for graphics transforms
   - Scale, rotate, translate operations
   - World-to-camera transformations

### Key Conventions

- **Radix Notation**: Numbers use "sM.N" notation where:
  - `s` = signed, `u` = unsigned
  - `M` = integer bits
  - `N` = fractional bits
  - Example: `s11.4` = signed, 11 integer bits, 4 fractional bits

- **Macro Naming**: All macros are UPPERCASE with `FR_` prefix
  - Example: `FR_ADD()`, `FR_MUL()`, `FR_ABS()`

- **Type Safety**: Use typedef'd types (`s8`, `s16`, `s32`, etc.) not raw C types

### Fixed-Point Operations

- **Alignment**: Addition/subtraction require aligned radix points
- **Multiplication**: Results in M+N bit precision (may need truncation)
- **Saturation**: Library supports saturating math to prevent overflow
- **Conversions**: Use `FR_CHRDX()` to change between radix representations

## Dependencies

The library has NO external dependencies - it compiles standalone on any C compiler without floating-point libraries.

## Testing

- Test file: `fr_math_test.c`
- Uses Travis CI for continuous integration (`.travis.yml`)
- Coverage reporting via lcov and coveralls