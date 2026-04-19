# PlatformIO Registry — Publishing Checklist

## One-time setup

### 1. Install PlatformIO CLI

```bash
pip install platformio
```

### 2. Create a PlatformIO account

```bash
pio account register -u deftio -e deftio@deftio.com \
    --firstname Manu --lastname Chatterjee
```

Confirm via the email verification link. Alternatively, register at
https://community.platformio.org/ (same account system).

### 3. Log in

```bash
pio account login -u deftio
```

### 4. Preview the package (optional but recommended)

```bash
cd /path/to/fr_math
pio pkg pack
```

Creates a `.tar.gz` showing exactly what will be published. The
`export.include` array in `library.json` controls which files are
packaged.

### 5. Publish for the first time

```bash
pio pkg publish --owner deftio
```

The library will appear at:
`https://registry.platformio.org/libraries/deftio/FR_Math`

## Subsequent releases (manual or CI)

PlatformIO does **NOT** auto-update from GitHub tags. You must
explicitly publish each new version.

### Option A: Manual CLI

```bash
# After bumping version in library.json and tagging:
pio pkg publish
```

### Option B: GitHub Actions (recommended)

1. Generate an API token:
   ```bash
   pio account token
   ```

2. Store the token as a GitHub repository secret named
   `PLATFORMIO_AUTH_TOKEN`.

3. The workflow `.github/workflows/release.yml` (or a dedicated
   `platformio-publish.yml`) should include:

   ```yaml
   - name: Install PlatformIO
     run: pip install platformio

   - name: Publish to PlatformIO
     env:
       PLATFORMIO_AUTH_TOKEN: ${{ secrets.PLATFORMIO_AUTH_TOKEN }}
     run: pio pkg publish --owner deftio --no-interactive
   ```

   Trigger on `release: types: [published]` or on tag push.

## Pre-publish validation checklist

### library.json

- [ ] `name` is `"FR_Math"`
- [ ] `version` matches `FR_MATH_VERSION` in `src/FR_math.h`
- [ ] `description` is clear and under 255 chars
- [ ] `keywords` array has relevant terms
- [ ] `repository.url` is `https://github.com/deftio/fr_math.git`
- [ ] `license` is `"BSD-2-Clause"`
- [ ] `frameworks` is `"*"` and `platforms` is `"*"`
- [ ] `build.srcFilter` includes `"+<FR_math.c>"`
- [ ] `build.includeDir` is `"src"`
- [ ] `export.include` lists `src/*`, `examples/*`, `LICENSE.txt`,
      `README.md`, `library.properties`

### Source

- [ ] No build artifacts (run `make clean`)
- [ ] `src/FR_math.h` has correct version in `FR_MATH_VERSION_HEX`

## Gotchas

- Once a name+version is published, it can **never** be reused, even
  after `pio pkg unpublish`.
- The `version` in `library.json` is authoritative, not the Git tag.
- If `--owner` is omitted, it defaults to the logged-in account.
- Publishing is instant — no approval process or PR review.

## Useful links

- `pio pkg publish` docs: https://docs.platformio.org/en/latest/core/userguide/pkg/cmd_publish.html
- `pio account` docs: https://docs.platformio.org/en/latest/core/userguide/account/cmd_register.html
- PlatformIO registry: https://registry.platformio.org/
