#!/usr/bin/env bash
# Install build tooling the distro cannot provide at the required version:
# recent CMake, Ninja, and Qt 6.5.x via aqtinstall.
#
# Usage: ci/linux/scripts/build-tools.sh
#
# Overridable env:
#   JOBS=1
#   QT_ROOT=/opt/qt
#   CMAKE_VERSION NINJA_VERSION AQTINSTALL_VERSION QT_VERSION
#     (Docker ARG/ENV, or ci/linux/versions.env for local runs)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=build-common.sh
source "${SCRIPT_DIR}/build-common.sh"

source_versions_env
require_env CMAKE_VERSION NINJA_VERSION AQTINSTALL_VERSION QT_VERSION

QT_ROOT="${QT_ROOT:-/opt/qt}"
QT_GCC="${QT_ROOT}/${QT_VERSION}/gcc_64"
QT6_DIR="${QT_GCC}/lib/cmake/Qt6"

log "Qt root: ${QT_ROOT} (version ${QT_VERSION})"
log "Jobs:    ${JOBS}"

# pip_install pkgs... — PEP 668 (Ubuntu 24.04+): try --break-system-packages, else plain (Rocky 9).
pip_install() {
    python3 -m pip install --no-cache-dir --break-system-packages "$@" \
        || python3 -m pip install --no-cache-dir "$@"
}

log "Installing CMake ${CMAKE_VERSION} + Ninja ${NINJA_VERSION} + aqtinstall ${AQTINSTALL_VERSION}"
pip_install \
    "cmake==${CMAKE_VERSION}" \
    "ninja==${NINJA_VERSION}" \
    "aqtinstall==${AQTINSTALL_VERSION}"

hash -r
log "Toolchain versions:"
cmake --version | head -1
ninja --version

# --- Qt SDK (official mirrors, no Qt account) ---------------------------------
if [ -d "${QT6_DIR}" ]; then
    log "Qt already present at ${QT_GCC} (skipping aqtinstall)"
else
    log "Installing Qt ${QT_VERSION} (gcc_64 + qtimageformats + qtpdf) into ${QT_ROOT}"
    python3 -m aqt install-qt linux desktop "${QT_VERSION}" gcc_64 \
        --modules qtimageformats qtpdf \
        --outputdir "${QT_ROOT}"
fi

if [ ! -d "${QT6_DIR}" ]; then
    echo "ERROR: Qt6 cmake package not found at ${QT6_DIR}" >&2
    exit 1
fi
log "Using Qt6_DIR=${QT6_DIR}"
