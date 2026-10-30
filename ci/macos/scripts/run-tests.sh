#!/usr/bin/env bash
# Run C++ unit tests (ctest). Skips Python tests (need a live xstudio session).
#
# Usage: ci/macos/scripts/run-tests.sh

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MACOS_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
REPO_ROOT="$(cd "${MACOS_DIR}/../.." && pwd)"
BUILD_DIR="${REPO_ROOT}/build"

log() { printf '\n\033[1;34m==> %s\033[0m\n' "$*"; }

if [ ! -d "$BUILD_DIR" ]; then
  echo "ERROR: build tree not found at ${BUILD_DIR}" >&2
  echo "  Run ci/macos/scripts/build-xstudio.sh first." >&2
  exit 1
fi

export JOBS="${JOBS:-1}"

log "Running ctest in ${BUILD_DIR}"
ctest --test-dir "$BUILD_DIR" --output-on-failure -j "${JOBS}" -E 'python_tests'

log "macOS tests OK"
