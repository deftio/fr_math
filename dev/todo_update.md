# FR_Math 2025 Update TODO

## Modernization Tasks

### CI/CD Migration
- [ ] Remove Travis CI configuration (.travis.yml)
- [ ] Implement GitHub Actions workflow for:
  - [ ] Build and test on multiple platforms (Linux, macOS, Windows)
  - [ ] Cross-compile tests for ARM/RISC-V targets
  - [ ] Coverage reporting with codecov or coveralls
  - [ ] Automated release builds

### Build System Improvements
- [ ] Add CMake support (keep Makefile for embedded targets)
- [ ] Clean build artifacts from repository (.o, .exe, .gcda, .gcno files)
- [ ] Add .gitignore for build artifacts
- [ ] Create separate examples/ directory for demo programs

## Core Feature Additions

### Missing Trigonometric Functions (High Priority)
- [ ] Complete inverse trig implementations:
  - [ ] FR_acos() - binary search implementation exists in TODO
  - [ ] FR_asin() 
  - [ ] FR_atan()
  - [ ] FR_atan2() - critical for angle calculations
- [ ] Add FR_tanFR() - fixed radix version of tangent

### Fast Distance Calculator (Patent US6502118B1 - Expired)
- [ ] Implement Chatterjee fast distance approximation algorithm
  - [ ] 2D distance: FR_dist2D(x1, y1, x2, y2, radix)
  - [ ] 3D distance: FR_dist3D(x1, y1, z1, x2, y2, z2, radix)
  - [ ] Manhattan distance variant
  - [ ] Euclidean approximation with configurable precision
- [ ] Add benchmarks comparing to sqrt-based methods
- [ ] Document precision/performance trade-offs

### Mathematical Operations
- [ ] FR_sqrt() - Newton-Raphson or binary search
- [ ] FR_inv_sqrt() - Fast inverse square root
- [ ] FR_div() - Fixed-point division with saturation
- [ ] FR_mod() - Optimized modulo for common angles (360, 2π)
- [ ] FR_pow() - Power function via log2/exp2

### Interpolation Suite
- [ ] FR_INTERP_LINEAR() - Linear interpolation macro
- [ ] FR_INTERP_COS() - Cosine interpolation for smoothing
- [ ] FR_INTERP_CUBIC() - Cubic interpolation
- [ ] FR_LERP2D() - 2D bilinear interpolation
- [ ] FR_INTERP_TABLE() - Table-based interpolation

### Wave Generation Functions
- [ ] FR_squareWaveS() - Square wave based on sin phase
- [ ] FR_squareWaveC() - Square wave based on cos phase
- [ ] FR_triangleWaveS() - Triangle wave
- [ ] FR_triangleWaveC() - Triangle wave (cos phase)
- [ ] FR_sawtoothWave() - Sawtooth wave generator
- [ ] FR_pwm() - PWM wave with configurable duty cycle

## Testing & Quality

### Test Coverage Expansion
- [ ] Complete test coverage for all existing functions
- [ ] Add precision/accuracy tests comparing to double precision
- [ ] Performance benchmarks for embedded targets
- [ ] Edge case testing (overflow, underflow, saturation)
- [ ] Create JSON error measurement output
- [ ] HTML test report generator

### Documentation
- [ ] API reference for all functions/macros
- [ ] Performance characteristics table
- [ ] Precision guarantees for each operation
- [ ] Migration guide from floating point
- [ ] Embedded platform optimization guide

## Architecture Improvements

### Code Organization
- [ ] Separate integer-only trig functions to FR_trig_degrees.h
- [ ] Create FR_constants.h for mathematical constants
- [ ] Optional namespace for C++ (FR::math)
- [ ] Consistent naming: lowercase functions (FR_sin not FR_Sin)

### Platform Optimizations
- [ ] ARM NEON intrinsics for batch operations
- [ ] RISC-V vector extension support
- [ ] Optional SIMD paths for x86 (SSE/AVX)
- [ ] Cortex-M optimized assembly variants

## Advanced Features (Future)

### Extended Precision
- [ ] 64-bit fixed-point operations
- [ ] Arbitrary precision with dynamic allocation
- [ ] Mixed-precision pipeline support

### Domain-Specific Extensions
- [ ] FR_fft() - Fixed-point FFT
- [ ] FR_filter() - FIR/IIR filter implementations
- [ ] FR_matrix() - Matrix operations
- [ ] FR_quaternion() - Quaternion math for rotations

## Notes

### Design Principles
- Maintain macro-based architecture for code size efficiency
- Zero dependencies policy
- Stability over features (no div-by-zero, graceful saturation)
- Document precision/performance trade-offs explicitly

### Clever Optimizations to Preserve
- Bit-shift approximations for π, e multiplication
- Binary search for inverse trig functions
- Quadrant-based table lookup for trig functions
- Mixed radix support throughout

### Patent Note
The Chatterjee fast distance calculation method (US6502118B1) provides approximation of Euclidean distance using only integer operations, shifts, and adds - perfect for this library's philosophy. Patent expired 2021, now public domain.