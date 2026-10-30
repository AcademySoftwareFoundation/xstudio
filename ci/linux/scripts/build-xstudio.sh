#!/usr/bin/env bash
# Configure and build xSTUDIO with vcpkg (USE_VCPKG) inside the toolchain image
# or on a matching host devenv.
#
# Usage: ci/linux/scripts/build-xstudio.sh
#
# Required env:
#   VCPKG_ROOT
#   VCPKG_TARGET_TRIPLET
#
# Overridable env:
#   JOBS=1
#   XSTUDIO_SRC=/src/xstudio
#   QT_ROOT=/opt/qt
#   QT_VERSION
#   CMAKE_PRESET=LinuxUbuntu2404NinjaRelease | LinuxRocky9NinjaRelease | …
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

: "${VCPKG_ROOT:?VCPKG_ROOT is required}"
: "${VCPKG_TARGET_TRIPLET:?VCPKG_TARGET_TRIPLET is required}"

SRC="${XSTUDIO_SRC:-/src/xstudio}"
QT_ROOT="${QT_ROOT:-/opt/qt}"
QT_GCC="${QT_ROOT}/${QT_VERSION}/gcc_64"
QT6_DIR="${QT_GCC}/lib/cmake/Qt6"

case "${VCPKG_TARGET_TRIPLET}" in
  x64-xstudio-linux-ubuntu2404)
    CMAKE_PRESET="${CMAKE_PRESET:-LinuxUbuntu2404NinjaRelease}"
    ;;
  x64-xstudio-linux-rocky9)
    CMAKE_PRESET="${CMAKE_PRESET:-LinuxRocky9NinjaRelease}"
    ;;
  *)
    echo "ERROR: unsupported VCPKG_TARGET_TRIPLET=${VCPKG_TARGET_TRIPLET}" >&2
    echo "  CI expects x64-xstudio-linux-ubuntu2404 or x64-xstudio-linux-rocky9." >&2
    exit 1
    ;;
esac

if [ ! -f "${SRC}/CMakeLists.txt" ]; then
  echo "ERROR: no xSTUDIO sources at ${SRC} (expected CMakeLists.txt)." >&2
  exit 1
fi
if [ ! -d "${QT6_DIR}" ]; then
  echo "ERROR: Qt6 cmake package not found at ${QT6_DIR}" >&2
  exit 1
fi
if [ ! -x "${VCPKG_ROOT}/vcpkg" ]; then
  echo "ERROR: vcpkg not found at ${VCPKG_ROOT}/vcpkg" >&2
  exit 1
fi

# Presets resolve CMAKE_TOOLCHAIN_FILE as ${sourceDir}/../vcpkg/... — require sibling layout.
expected_sibling="$(cd "${SRC}/.." && pwd)/vcpkg"
if [ "$(cd "${VCPKG_ROOT}" && pwd)" != "${expected_sibling}" ]; then
  echo "ERROR: VCPKG_ROOT must be the sibling '${expected_sibling}' (CMakePresets toolchain path)." >&2
  echo "  Got: ${VCPKG_ROOT}" >&2
  exit 1
fi

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

log "Configuring xSTUDIO with preset ${CMAKE_PRESET}"
# Deps are installed by build-deps.sh into ${SRC}/vcpkg_installed. Manifest
# install stays OFF so cmake does not rebuild ports (sccache launchers would
# change the ABI hash). Point VCPKG_INSTALLED_DIR at that tree — the toolchain
# default is ${CMAKE_BINARY_DIR}/vcpkg_installed, which is empty here.
# Do not export CMAKE_*_COMPILER_LAUNCHER into the environment (breaks pip's
# isolated cmake); pass them only as cache vars for the xSTUDIO compile.
# GCC 13+: keep -Werror but ignore maybe-uninitialized (Imath inlines).
env -u CMAKE_C_COMPILER_LAUNCHER -u CMAKE_CXX_COMPILER_LAUNCHER \
cmake -S "${SRC}" -B "${SRC}/build" \
  --preset "${CMAKE_PRESET}" \
  -DQt6_DIR="${QT6_DIR}" \
  -DCMAKE_CXX_FLAGS="-Wno-error=maybe-uninitialized" \
  -DBMD_DECKLINK_PLUGIN="${BMD_DECKLINK:-OFF}" \
  -DBUILD_TESTING="${BUILD_TESTING:-OFF}" \
  -DVCPKG_TARGET_TRIPLET="${VCPKG_TARGET_TRIPLET}" \
  -DVCPKG_MANIFEST_INSTALL=OFF \
  -DVCPKG_INSTALLED_DIR="${SRC}/vcpkg_installed" \
  "${cmake_extra[@]}" \
  "${cmake_launcher_args[@]}"

log "Building xSTUDIO"
cmake --build "${SRC}/build" -j "${JOBS}"

log "Build OK"
