#!/usr/bin/env bash
# Run C++ unit tests (ctest). Skips Python tests (need a live xstudio session).
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=lib.sh
source "$HERE/lib.sh"

SRC="${XSTUDIO_SRC:-/src/xstudio}"
BUILD_DIR="${SRC}/build"

if [ ! -d "$BUILD_DIR" ]; then
    echo "ERROR: build tree not found at ${BUILD_DIR}" >&2
    exit 1
fi

log "Running ctest in ${BUILD_DIR}"
ctest --test-dir "$BUILD_DIR" --output-on-failure -j "${JOBS:-1}" -E 'python_tests'

log "Tests OK"
