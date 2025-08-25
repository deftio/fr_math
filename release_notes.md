# FR_Math Release Notes

## Version 1.0.3 (2025)

### Test Coverage Improvements
- **Increased overall test coverage from 4% to 72%**
  - FR_math.c: 60% coverage
  - FR_math_2D.cpp: 98% coverage
- Added comprehensive test suite with multiple test files:
  - `test_comprehensive.c` - Comprehensive tests for core math functions
  - `test_overflow_saturation.c` - Edge case and overflow behavior tests
  - `test_full_coverage.c` - Tests for previously uncovered functions
  - `test_2d_complete.cpp` - Complete 2D transformation matrix tests
- Fixed test failures in 2D transformation tests (XFormPtI now correctly expects integer inputs)

### Bug Fixes
- **Fixed FR_atan function** - Was incorrectly calling FR_atan2 with wrong arguments, now properly calls FR_atanI
- **Fixed constant declarations** - Changed FR_PI, FR_2PI, FR_E from non-const to proper const declarations
- **Fixed XFormPtI test assumptions** - Tests were incorrectly passing fixed-point values instead of integers

### Build System Enhancements
- **Added comprehensive Makefile targets**:
  - `make lib` - Build static library
  - `make test` - Run all tests
  - `make coverage` - Generate coverage reports with lcov
  - `make examples` - Build example programs
  - `make clean` - Clean build artifacts
- **Added GitHub Actions CI/CD**:
  - Multi-platform testing (Ubuntu, macOS)
  - Multi-compiler support (gcc, clang)
  - Cross-compilation testing (ARM, RISC-V)
  - 32-bit compatibility testing
  - Automated coverage reporting to Codecov
  - Overflow and saturation testing with sanitizers

### Documentation Improvements
- **Cleaned up README.md**:
  - Fixed grammar and spelling throughout
  - Added "Building and Testing" section with clear instructions
  - Improved formatting with consistent markdown usage
  - Added supported platforms list
  - Better code examples with syntax highlighting
  - Clearer mathematical notation and explanations
- **Added CLAUDE.md** - Assistant-friendly documentation with:
  - Project structure overview
  - Key concepts and math notation explained
  - Testing and linting commands
  - Common pitfalls and tips
- **Added inline documentation** for test files explaining coverage goals

### Code Quality
- **Removed unused/unimplemented functions**:
  - Removed FR_atan declaration (was declared but never implemented)
  - Cleaned up test code to remove references to non-existent functions
- **Fixed compiler warnings**:
  - Resolved deprecated C++ warnings
  - Fixed format string warnings in tests
  - Addressed unused variable warnings
- **Improved code organization**:
  - Better separation of test types
  - Clearer test naming conventions
  - More maintainable test structure

### Platform Support
- Verified compilation and testing on:
  - x86/x64 (Linux, macOS)
  - ARM (32-bit and 64-bit)
  - RISC-V
  - 32-bit x86 targets
- Added cross-compilation support in CI

### Version Updates
- Updated all source file headers to version 1.0.3
- Added version section to README.md

## Previous Versions

### Version 1.02
- Initial public release
- Basic fixed-point math operations
- 2D transformation matrices
- Core trigonometric functions

### Version 1.01
- Internal development version
- Cleaned up naming conventions
- Initial test framework

---

*Note: FR_Math has been in development since 2000, originally used in embedded systems for Palm Pilots and later ARM cores. This is the first version with comprehensive testing and CI/CD integration.*