#!/usr/bin/env bash
# Configure and build xSTUDIO from sources already present at $XSTUDIO_SRC
# (Docker COPY, or bind-mounted by ci/linux/scripts/run-build-container.sh).
# Build check only — no install/package.
#
# Usage: ci/linux/scripts/build-xstudio.sh
#
# Overridable env:
#   JOBS=1
#   XSTUDIO_SRC=/src/xstudio
#   QT_ROOT=/opt/qt
#   QT_VERSION          (Docker ENV, or ci/linux/versions.env for local runs)
#   BUILD_TESTING=OFF
#   BMD_DECKLINK=OFF
#   STUDIO_PLUGINS=
#   CMAKE_C_COMPILER_LAUNCHER / CMAKE_CXX_COMPILER_LAUNCHER (e.g. sccache)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=build-common.sh
source "${SCRIPT_DIR}/build-common.sh"

source_versions_env
require_env QT_VERSION

SRC="${XSTUDIO_SRC:-/src/xstudio}"
QT_ROOT="${QT_ROOT:-/opt/qt}"
QT_GCC="${QT_ROOT}/${QT_VERSION}/gcc_64"
QT6_DIR="${QT_GCC}/lib/cmake/Qt6"

if [ ! -f "${SRC}/CMakeLists.txt" ]; then
    echo "ERROR: no xSTUDIO sources at ${SRC} (expected CMakeLists.txt)." >&2
    echo "  Mount or COPY the repo into ${SRC}." >&2
    exit 1
fi
if [ ! -d "${QT6_DIR}" ]; then
    echo "ERROR: Qt6 cmake package not found at ${QT6_DIR}" >&2
    exit 1
fi

PYBIND11_CMAKE_DIR="$(python3 -m pybind11 --cmakedir 2>/dev/null || true)"
if [ -z "${PYBIND11_CMAKE_DIR}" ] || [ ! -d "${PYBIND11_CMAKE_DIR}" ]; then
    echo "ERROR: pip pybind11 cmake dir not found (python3 -m pybind11 --cmakedir)." >&2
    echo "  Run ci/linux/scripts/build-tools.sh first." >&2
    exit 1
fi
log "Using pip pybind11 cmake dir: ${PYBIND11_CMAKE_DIR}"

cmake_extra=()
if [ -n "${STUDIO_PLUGINS:-}" ]; then
    cmake_extra+=(-DSTUDIO_PLUGINS="${STUDIO_PLUGINS}")
fi

cmake_launcher_args=()
if [ -n "${CMAKE_C_COMPILER_LAUNCHER:-}" ]; then
    cmake_launcher_args+=(-DCMAKE_C_COMPILER_LAUNCHER="${CMAKE_C_COMPILER_LAUNCHER}")
fi
if [ -n "${CMAKE_CXX_COMPILER_LAUNCHER:-}" ]; then
    cmake_launcher_args+=(-DCMAKE_CXX_COMPILER_LAUNCHER="${CMAKE_CXX_COMPILER_LAUNCHER}")
fi

log "Configuring xSTUDIO from ${SRC}"
# GCC 13: keep -Werror but ignore maybe-uninitialized (Imath inlines).
cmake -S "${SRC}" -B "${SRC}/build" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="${QT_GCC}" \
    -DCMAKE_CXX_FLAGS="-Wno-error=maybe-uninitialized" \
    -Dpybind11_DIR="${PYBIND11_CMAKE_DIR}" \
    -DBMD_DECKLINK="${BMD_DECKLINK:-OFF}" \
    -DBUILD_TESTING="${BUILD_TESTING:-OFF}" \
    "${cmake_extra[@]}" \
    "${cmake_launcher_args[@]}"

log "Building xSTUDIO"
cmake --build "${SRC}/build" -j "${JOBS}"

log "Build OK"
