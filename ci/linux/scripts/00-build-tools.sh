#!/usr/bin/env bash
# Install build tooling the distro cannot provide at the required version:
# recent CMake, Ninja, pybind11 2.13.x, and Qt 6.5.x via aqtinstall.
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=lib.sh
source "$HERE/lib.sh"

require_env CMAKE_VERSION NINJA_VERSION PYBIND11_VERSION AQTINSTALL_VERSION QT_VERSION QT_ROOT

# --- 1) CMake + Ninja + pybind11 + aqtinstall (pip) --------------------------
log "Installing CMake ${CMAKE_VERSION} + Ninja ${NINJA_VERSION} + pybind11 ${PYBIND11_VERSION}"
# PEP 668 (Ubuntu 24.04+): PIP_BREAK_SYSTEM_PACKAGES / --break-system-packages.
# Rocky 9 ignores the flag -> fallback without it. Do not upgrade system pip.
PIP_FLAGS="--no-cache-dir"
export PIP_BREAK_SYSTEM_PACKAGES=1
python3 -m pip install $PIP_FLAGS --break-system-packages \
        "cmake==${CMAKE_VERSION}" "ninja==${NINJA_VERSION}" "pybind11==${PYBIND11_VERSION}" "aqtinstall==${AQTINSTALL_VERSION}" \
    || python3 -m pip install $PIP_FLAGS \
        "cmake==${CMAKE_VERSION}" "ninja==${NINJA_VERSION}" "pybind11==${PYBIND11_VERSION}" "aqtinstall==${AQTINSTALL_VERSION}"

hash -r
log "Toolchain versions:"
cmake --version | head -1
ninja --version
echo "pybind11: $(python3 -m pybind11 --version 2>/dev/null || echo "${PYBIND11_VERSION}")"

# --- 2) Qt SDK (official mirrors, no Qt account) -----------------------------
log "Installing Qt ${QT_VERSION} (gcc_64 + qtimageformats + qtpdf) into ${QT_ROOT} via aqtinstall"
# qtpdf: optional PDF media-reader plugin (Qt6::Pdf).
python3 -m aqt install-qt linux desktop "${QT_VERSION}" gcc_64 \
        --modules qtimageformats qtpdf \
        --outputdir "${QT_ROOT}"

QT_GCC="${QT_ROOT}/${QT_VERSION}/gcc_64"
if [ ! -d "${QT_GCC}/lib/cmake/Qt6" ]; then
    echo "ERROR: Qt6 cmake package not found in ${QT_GCC}" >&2
    exit 1
fi
log "Qt installed at ${QT_GCC}"
