#!/usr/bin/env bash
#
# codeql_local.sh — run the same CodeQL analysis as GitHub code scanning,
# locally, before pushing.
#
# GitHub's default setup for this repo scans c-cpp, actions, python and
# javascript with the "default" query suite (codeql-suites/<lang>-code-
# scanning.qls) and no build (build-mode none). This script does the same
# on a copy of the files git would push (tracked + untracked-not-ignored),
# so local-only trees such as dist/ or third-party checkouts under
# compare_lfm/ are not scanned.
#
# Usage:
#   scripts/codeql_local.sh               # default suite, all languages
#   scripts/codeql_local.sh --extended    # security-extended suite
#   CODEQL_LANGS="c-cpp" scripts/codeql_local.sh
#
# Needs the CodeQL CLI (https://github.com/github/codeql-cli-binaries;
# on macOS: brew install --cask codeql). Query packs (codeql/<lang>-queries,
# published by GitHub) are downloaded on first use if missing.
#
# Output: build/codeql/<lang>.sarif, plus a one-line summary per finding.
# Exit status: 0 if no findings, 1 if any, 2 on tool errors.
#
# Not covered: GitHub's "AI Scan" alerts have no local equivalent. The
# sanitizer, shift-check and every-input test targets (make test-shift,
# make test-exhaustive) are the local backstop for that class of bug.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
OUT="${ROOT}/build/codeql"
LANGS="${CODEQL_LANGS:-c-cpp actions python javascript}"
SUITE="code-scanning"

for arg in "$@"; do
    case "$arg" in
        --extended) SUITE="security-extended" ;;
        -h|--help) sed -n '3,27p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "Unknown option: $arg" >&2; exit 2 ;;
    esac
done

if ! command -v codeql >/dev/null 2>&1; then
    echo "codeql not found. Install the CodeQL CLI (macOS: brew install --cask codeql)." >&2
    exit 2
fi

# CodeQL names the C/C++ query pack "cpp".
pack_of() { if [ "$1" = "c-cpp" ]; then echo cpp; else echo "$1"; fi; }

for lang in ${LANGS}; do
    pack="codeql/$(pack_of "$lang")-queries"
    if ! codeql resolve packs 2>/dev/null | grep -q "${pack}:"; then
        echo "Downloading ${pack} ..."
        codeql pack download "${pack}" >/dev/null || exit 2
    fi
done

# Snapshot the files git would push.
rm -rf "${OUT}"
mkdir -p "${OUT}/src"
(cd "${ROOT}" && git ls-files -co --exclude-standard -z) \
    | (cd "${ROOT}" && rsync -a --from0 --files-from=- ./ "${OUT}/src/")

total=0
for lang in ${LANGS}; do
    q="$(pack_of "$lang")"
    echo "== CodeQL ${lang} (${SUITE})"
    codeql database create "${OUT}/db-${lang}" --language="${lang}" \
        --build-mode=none --source-root="${OUT}/src" --overwrite \
        >"${OUT}/create-${lang}.log" 2>&1 \
        || { echo "database create failed, see ${OUT}/create-${lang}.log" >&2; exit 2; }
    codeql database analyze "${OUT}/db-${lang}" \
        "codeql/${q}-queries:codeql-suites/${q}-${SUITE}.qls" \
        --format=sarif-latest --output="${OUT}/${lang}.sarif" \
        >"${OUT}/analyze-${lang}.log" 2>&1 \
        || { echo "analyze failed, see ${OUT}/analyze-${lang}.log" >&2; exit 2; }
    n=$(python3 - "${OUT}/${lang}.sarif" <<'EOF'
import json, sys
results = json.load(open(sys.argv[1]))["runs"][0]["results"]
for r in results:
    loc = r["locations"][0]["physicalLocation"]
    print("   %s  %s:%s  %s" % (r["ruleId"], loc["artifactLocation"]["uri"],
          loc.get("region", {}).get("startLine", "?"),
          r["message"]["text"].splitlines()[0][:100]), file=sys.stderr)
print(len(results))
EOF
)
    echo "   ${n} finding(s)"
    total=$((total + n))
done

echo "CodeQL: ${total} finding(s). SARIF in build/codeql/."
[ "${total}" -eq 0 ]
