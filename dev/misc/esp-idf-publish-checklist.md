# ESP-IDF Component Registry — Publishing Checklist

## One-time setup

### 1. Log in to the registry

Visit https://components.espressif.com/users/login and authenticate with
your GitHub account. A namespace matching your GitHub username (`deftio`)
is created automatically.

### 2. Install the `compote` CLI

```bash
pip install idf-component-manager
```

This gives you the `compote` command (standalone — does not require a
full ESP-IDF installation).

### 3. Preview the package

```bash
cd /path/to/fr_math
compote component pack --name fr_math
```

This creates an archive in `dist/` so you can inspect what will be uploaded.

### 4. Upload for the first time

Authentication happens automatically during upload (triggers OAuth on
first use). The token is stored in
`~/.espressif/idf_component_manager.yml`.

```bash
compote component upload --name fr_math --namespace deftio
```

The component will appear at:
`https://components.espressif.com/components/deftio/fr_math`

## Subsequent releases (manual or CI)

ESP-IDF does **NOT** auto-update from GitHub tags. You must explicitly
upload each new version.

### Option A: Manual CLI

```bash
# After bumping version in idf_component.yml:
compote component upload --name fr_math --namespace deftio
```

### Option B: GitHub Actions with API token

1. Log in at https://components.espressif.com
2. Go to your username dropdown > Tokens settings
3. Create a new API token
4. Store it as a GitHub secret named `IDF_COMPONENT_API_TOKEN`

Workflow snippet:
```yaml
- name: Upload to ESP Component Registry
  uses: espressif/upload-components-ci-action@v2
  with:
    components: "fr_math:."
    namespace: "deftio"
    api_token: ${{ secrets.IDF_COMPONENT_API_TOKEN }}
```

Trigger on `release: types: [published]` or on tag push.

### Option C: GitHub Actions with OIDC (no stored secret)

```yaml
jobs:
  upload:
    runs-on: ubuntu-latest
    permissions:
      id-token: write
      contents: read
    steps:
      - uses: actions/checkout@v4
      - name: Upload to ESP Component Registry
        uses: espressif/upload-components-ci-action@v2
        with:
          components: "fr_math:."
          namespace: "deftio"
```

OIDC eliminates long-lived secrets. GitHub generates a short-lived token
per workflow run. Requires that your namespace on the ESP registry is
linked to your GitHub account (default if you logged in via GitHub).

## Pre-publish validation checklist

### idf_component.yml

- [ ] `version` matches `FR_MATH_VERSION` in `src/FR_math.h`
- [ ] `description` is clear
- [ ] `url` is `https://github.com/deftio/fr_math`
- [ ] `repository` is `https://github.com/deftio/fr_math.git`
- [ ] `license` is `BSD-2-Clause`
- [ ] `targets` lists relevant ESP32 variants

### CMakeLists.txt

- [ ] `if(ESP_PLATFORM)` guard is present
- [ ] `idf_component_register(SRCS "src/FR_math.c" INCLUDE_DIRS "src")`
- [ ] Does not break non-ESP builds

### Source

- [ ] No build artifacts (run `make clean`)
- [ ] `src/FR_math.h` has correct version in `FR_MATH_VERSION_HEX`

## How users consume it

Once published, ESP-IDF users add the dependency:

```bash
idf.py add-dependency "deftio/fr_math^2.0.2"
```

Or add to their project's `idf_component.yml`:
```yaml
dependencies:
  deftio/fr_math: "^2.0.2"
```

Then `#include "FR_math.h"` in their code.

## Gotchas

- Component name in the registry comes from the `--name` flag during
  upload, not from the YAML file. Use lowercase with underscores.
- The `version` in `idf_component.yml` is authoritative. Alternatively,
  pass `--version git` to derive version from the Git tag.
- Versions are immutable once uploaded.
- Custom namespaces (other than your GitHub username) require manual
  approval from Espressif.
- The OIDC approach (Option C) is preferred over stored API tokens.

## Useful links

- ESP Component Registry: https://components.espressif.com/
- Packaging guide: https://docs.espressif.com/projects/idf-component-manager/en/latest/
- upload-components-ci-action: https://github.com/espressif/upload-components-ci-action
- compote CLI: https://docs.espressif.com/projects/idf-component-manager/en/latest/reference/compote_cli.html
