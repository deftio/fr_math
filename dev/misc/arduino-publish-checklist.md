# Arduino Library Manager — Publishing Checklist

## One-time registration

1. Fork https://github.com/arduino/library-registry
2. In your fork, edit `repositories.txt` and add this line:
   ```
   https://github.com/deftio/fr_math
   ```
3. Open a PR from your fork to `arduino/library-registry` main branch.
4. The `@ArduinoBot` runs automated validation within minutes. It checks:
   - `library.properties` exists and follows the
     [Arduino Library 1.5 spec](https://arduino.github.io/arduino-cli/latest/library-specification/)
   - `name` is unique in the registry (case-insensitive)
   - `name` does not start with "Arduino"
   - At least one Git tag/release exists on the repo
   - No `.exe` files, no symlinks, no git submodules
   - No `.development` flag file
5. If the bot reports issues, fix them in `deftio/fr_math`, bump `version`
   in `library.properties`, create a new tag, then comment in the PR to
   re-trigger validation.
6. Once the PR is merged, the library appears in the Arduino Library
   Manager within ~24 hours.

## Subsequent releases (automatic)

The Arduino indexer polls all registered repos hourly. When it finds a
new Git tag whose `version` field in `library.properties` differs from
all previously indexed versions, the new version is indexed automatically.

Steps for each release:
1. Bump `FR_MATH_VERSION_HEX` in `src/FR_math.h`
2. Run `./scripts/sync_version.sh` (updates `library.properties` etc.)
3. Commit, tag (`git tag v2.0.3`), push (`git push && git push --tags`)
4. Done — Arduino picks it up within the hour.

Monitor indexing at:
`http://downloads.arduino.cc/libraries/logs/github.com/deftio/fr_math/`

## Pre-publish validation checklist

### library.properties

- [ ] `name=FR_Math` — matches desired Arduino Library Manager name
- [ ] `version` matches `FR_MATH_VERSION` in `src/FR_math.h`
- [ ] `author` and `maintainer` have name and email
- [ ] `sentence` is under 120 characters (shown in library list)
- [ ] `paragraph` provides additional detail
- [ ] `category` is a valid value: Communication, Display, Signal
      Input/Output, Sensors, Device Control, Timing, Data Storage,
      Data Processing, or **Other**
- [ ] `url` points to `https://github.com/deftio/fr_math`
- [ ] `architectures=*`
- [ ] `includes=FR_math.h`

### Source layout

- [ ] `src/FR_math.h` exists (main header)
- [ ] `src/FR_math.c` exists (implementation)
- [ ] `src/FR_defs.h` exists (type definitions)
- [ ] `src/FR_trig_table.h` exists (sine table)
- [ ] No build artifacts in `src/` (run `make clean` before tagging)
- [ ] No `.exe` files anywhere in the repo
- [ ] No symlinks (Arduino rejects them)

### Examples

- [ ] Each `.ino` file is in a subdirectory with a matching name:
  - `examples/basic-math/basic-math.ino`
  - `examples/trig-functions/trig-functions.ino`
  - `examples/wave-generators/wave-generators.ino`
  - `examples/arduino_smoke/arduino_smoke.ino`
- [ ] Each `.ino` uses `#include "FR_math.h"` (not relative paths)
- [ ] Non-Arduino examples (`.c`, `.cpp`) are in their own subdirectory
      and won't confuse the IDE

### Repository

- [ ] Repository is public
- [ ] At least one release tag exists (e.g., `v2.0.2`)
- [ ] `library.properties` is at the repo root

## Useful links

- Library specification: https://arduino.github.io/arduino-cli/latest/library-specification/
- Registry repo & FAQ: https://github.com/arduino/library-registry
- arduino-lint tool: https://arduino.github.io/arduino-lint/latest/
- Submission guide: https://support.arduino.cc/hc/en-us/articles/360012175419
