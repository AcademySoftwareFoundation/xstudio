#!/usr/bin/env bash
# Configure and build xSTUDIO on macOS (Apple Silicon).
# Expects deps already installed (ci/macos/scripts/build-deps.sh).
#
# Usage: ci/macos/scripts/build-xstudio.sh
#
# Overridable env:
#   JOBS=1
#   QT_ROOT=$HOME/Qt
#   VCPKG_ROOT=<sibling-of-repo>/vcpkg
#   BUILD_TESTING=OFF
#   CMAKE_C_COMPILER_LAUNCHER / CMAKE_CXX_COMPILER_LAUNCHER (e.g. sccache)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MACOS_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
REPO_ROOT="$(cd "${MACOS_DIR}/../.." && pwd)"

log() { printf '\n\033[1;34m==> %s\033[0m\n' "$*"; }

# shellcheck source=../versions.env
source "${MACOS_DIR}/versions.env"
: "${QT_VERSION:?QT_VERSION missing from ci/macos/versions.env}"

export JOBS="${JOBS:-1}"
QT_ROOT="${QT_ROOT:-${HOME}/Qt}"
VCPKG_ROOT="${VCPKG_ROOT:-$(cd "${REPO_ROOT}/.." && pwd)/vcpkg}"
QT6_DIR="${QT_ROOT}/${QT_VERSION}/macos/lib/cmake/Qt6"

if [ ! -d "${QT6_DIR}" ]; then
  echo "ERROR: Qt6 cmake package not found at ${QT6_DIR}" >&2
  echo "  Run ci/macos/scripts/build-deps.sh first." >&2
  exit 1
fi
if [ ! -x "${VCPKG_ROOT}/vcpkg" ]; then
  echo "ERROR: vcpkg not found at ${VCPKG_ROOT}" >&2
  echo "  Run ci/macos/scripts/build-deps.sh first." >&2
  exit 1
fi

cmake_launcher_args=()
if [ -n "${CMAKE_C_COMPILER_LAUNCHER:-}" ]; then
  cmake_launcher_args+=(-DCMAKE_C_COMPILER_LAUNCHER="${CMAKE_C_COMPILER_LAUNCHER}")
fi
if [ -n "${CMAKE_CXX_COMPILER_LAUNCHER:-}" ]; then
  cmake_launcher_args+=(-DCMAKE_CXX_COMPILER_LAUNCHER="${CMAKE_CXX_COMPILER_LAUNCHER}")
fi

log "Configuring with preset MacOSNinjaRelease"
cmake -S "${REPO_ROOT}" -B "${REPO_ROOT}/build" \
  --preset MacOSNinjaRelease \
  -DQt6_DIR="${QT6_DIR}" \
  -DBUILD_TESTING="${BUILD_TESTING:-OFF}" \
  "${cmake_launcher_args[@]}"

log "Building xSTUDIO"
cmake --build "${REPO_ROOT}/build" -j "${JOBS}"

log "macOS build OK"
