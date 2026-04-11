#!/usr/bin/env bash
#
# run.sh — build the Docker image (if needed) and run the size report.
#
# Usage:
#   ./docker/run.sh              # build image + run size report
#   ./docker/run.sh --rebuild    # force rebuild the image first
#
# Output: markdown size table to stdout + build/size_table.md

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
IMAGE_NAME="fr-math-sizes"

REBUILD=0
for arg in "$@"; do
    case "$arg" in
        --rebuild) REBUILD=1 ;;
        -h|--help)
            sed -n '3,11p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
            exit 0
            ;;
        *) echo "Unknown argument: $arg" >&2; exit 2 ;;
    esac
done

# Build image if it doesn't exist or --rebuild was passed.
if [[ "${REBUILD}" == "1" ]] || ! docker image inspect "${IMAGE_NAME}" >/dev/null 2>&1; then
    echo "Building Docker image '${IMAGE_NAME}'..."
    docker build --platform linux/amd64 -t "${IMAGE_NAME}" "${SCRIPT_DIR}"
    echo ""
fi

# Run the size report.
docker run --platform linux/amd64 --rm \
    -v "${PROJECT_ROOT}:/src" \
    "${IMAGE_NAME}" \
    bash /src/docker/build_sizes.sh
