# FR_Math Tools

Diagnostic and code-generation utilities for the FR_Math library.

## trig_neighborhood

Sweep any math function over a range and print a neighborhood table showing
raw output, expected reference, absolute error, and percent error.

**Build:** `make tools`

**Usage:**
```
trig_neighborhood <func> <center> <half> [options]
```

### Supported functions (25)

| Category | Functions |
|---|---|
| Trig (degrees) | `fr_sin_bam`, `fr_cos_bam`, `fr_tan_bam`, `fr_sin`, `fr_cos`, `fr_tan`, `FR_SinI`, `FR_CosI`, `FR_TanI`, `fr_sin_deg`, `fr_cos_deg`, `fr_tan_deg` |
| Inverse trig | `FR_acos`, `FR_asin`, `FR_atan`, `FR_atan2` |
| Logarithmic | `FR_log2`, `FR_ln`, `FR_log10` |
| Exponential | `FR_pow2`, `FR_EXP`, `FR_POW10` |
| Other | `FR_sqrt`, `FR_hypot`, `FR_hypot_fast8` |

### Options

| Option | Description | Default |
|---|---|---|
| `--inc <step>` | Increment per sample | function-dependent |
| `--fmt md\|csv\|ascii` | Output format | `md` |
| `--radix <r>` | Input radix for fixed-point | 16 |
| `--out_radix <r>` | Output radix (inv trig, log) | 16 |
| `--y <val>` | Fixed y for hypot functions | 0.0 |

### Default increments

- Trig + FR_atan2: `360/65536` (~0.0055 degrees)
- FR_acos, FR_asin: `1/32768` (~3.05e-5)
- All others: `1/65536` (~1.53e-5)

### Examples

```bash
# Cosine near -90 degrees
build/trig_neighborhood fr_cos -90 15

# Sine sweep in CSV format
build/trig_neighborhood fr_sin -360 10 --fmt csv

# Tangent near pole
build/trig_neighborhood fr_tan 89.5 20 --inc 0.01

# Arcsine near zero
build/trig_neighborhood FR_asin 0.0001 15 --inc 3.05e-5 --radix 15

# Log2 near 1.0
build/trig_neighborhood FR_log2 1.0 15 --inc 0.01

# Atan2 near 90 degrees
build/trig_neighborhood FR_atan2 90 15

# Hypot with y=50
build/trig_neighborhood FR_hypot_fast8 100 15 --y 50 --radix 8
```

---

## coef-gen.py

Python script for generating power-of-two coefficient approximations. Given a
target floating-point value, searches for combinations of `+/- 2^(-k)` terms
that best approximate the value using only bit-shifts and adds.

**Usage:** `python3 tools/coef-gen.py`

---

## fr_coef-gen.cpp

C++ coefficient generator for 32-bit host. Similar purpose to `coef-gen.py`
but runs natively and can be used for brute-force search over larger term
counts.

**Build:** `g++ -O2 tools/fr_coef-gen.cpp -o build/fr_coef-gen`

---

## gen_pow2_table.py

Generates the `gFR_POW2_FRAC_TAB[65]` lookup table used by `FR_pow2()`.
Output is a C array suitable for inclusion in FR_math.c.

**Usage:** `python3 tools/gen_pow2_table.py`

---

## gen_radix28_constants.py

Generates radix-28 constants used by FR_EXP, FR_ln, FR_log10 for base
conversion (e.g., `FR_kLOG2E_28`, `FR_kLOG2_10_28`).

**Usage:** `python3 tools/gen_radix28_constants.py`

---

## check_published_versions.sh

Verifies that published version tags match the version defined in
`FR_math.h` (`FR_MATH_VERSION_HEX`). Used in CI/release workflows.

**Usage:** `bash tools/check_published_versions.sh`

---

## make_release.sh

Release automation script. Bumps version, tags, and prepares release
artifacts.

**Usage:** `bash tools/make_release.sh`

---

## interp_analysis.html

Interactive HTML/JS visualization for interpolation analysis. Open in a
browser to explore interpolation error characteristics.

**Usage:** Open `tools/interp_analysis.html` in a web browser.
