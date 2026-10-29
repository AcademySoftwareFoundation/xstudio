#!/usr/bin/env bash
# Configure and build xSTUDIO from sources already present at $XSTUDIO_SRC
# (copied by the build Dockerfile). Build check only — no install/package.
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=lib.sh
source "$HERE/lib.sh"

require_env QT_VERSION QT_ROOT

SRC="${XSTUDIO_SRC:-/src/xstudio}"
QT_GCC="${QT_ROOT}/${QT_VERSION}/gcc_64"

# --- 1) Ensure the source tree was copied into the image --------------------
if [ ! -f "$SRC/CMakeLists.txt" ]; then
    echo "ERROR: no xSTUDIO sources at ${SRC} (expected CMakeLists.txt)." >&2
    echo "  The build Dockerfile must COPY the repo into ${SRC}." >&2
    exit 1
fi

log "Building xSTUDIO from ${SRC}"

# --- 2) Prefer pip pybind11 (>= 2.13) over the distro package ----------------
PYBIND11_CMAKE_DIR="$(python3 -m pybind11 --cmakedir)"
log "Using pip pybind11 cmake dir: ${PYBIND11_CMAKE_DIR}"

# --- 3) Optional CMake flags -------------------------------------------------
cmake_extra=()
if [ -n "${STUDIO_PLUGINS:-}" ]; then
    cmake_extra+=(-DSTUDIO_PLUGINS="${STUDIO_PLUGINS}")
fi

# --- 4) Configure ------------------------------------------------------------
# GCC 13: keep -Werror but ignore maybe-uninitialized (Imath inlines).
cmake -S "$SRC" -B "$SRC/build" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="$QT_GCC" \
    -DCMAKE_CXX_FLAGS="-Wno-error=maybe-uninitialized" \
    -Dpybind11_DIR="$PYBIND11_CMAKE_DIR" \
    -DBMD_DECKLINK_PLUGIN="${BMD_DECKLINK:-OFF}" \
    -DBUILD_TESTING="${BUILD_TESTING:-OFF}" \
    "${cmake_extra[@]}"

# --- 5) Build ----------------------------------------------------------------
log "Building xSTUDIO"
cmake --build "$SRC/build" -j "$JOBS"

log "Build OK"
