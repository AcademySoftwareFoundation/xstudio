#!/usr/bin/env bash
# Compile-check xSTUDIO on macOS (Apple Silicon).
#
# Mirrors the local build guide (docs/reference/build_guides/macos.md):
# Homebrew tooling, Qt via aqtinstall, vcpkg at the builtin-baseline, then
# cmake --preset MacOSNinjaRelease + build. No install / macdeployqt / tests.
#
# Usage (from repo root or anywhere):
#   ci/macos/build.sh
#
# Overridable env:
#   JOBS=3
#   QT_ROOT=$HOME/Qt
#   VCPKG_ROOT=<sibling-of-repo>/vcpkg
#   SKIP_BREW=1          # skip brew install (tools already present)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
VERSIONS_ENV="${SCRIPT_DIR}/versions.env"

log() { printf '\n\033[1;34m==> %s\033[0m\n' "$*"; }

# --- 1) Pinned versions -------------------------------------------------------
# shellcheck source=versions.env
source "${VERSIONS_ENV}"

: "${QT_VERSION:?QT_VERSION missing from ci/macos/versions.env}"
: "${AQTINSTALL_VERSION:?AQTINSTALL_VERSION missing from ci/macos/versions.env}"

VCPKG_BASELINE="$(
  python3 -c "import json,sys; print(json.load(open(sys.argv[1]))['builtin-baseline'])" \
    "${REPO_ROOT}/vcpkg.json"
)"
: "${VCPKG_BASELINE:?could not read builtin-baseline from vcpkg.json}"

export JOBS="${JOBS:-3}"
QT_ROOT="${QT_ROOT:-${HOME}/Qt}"
# Preset CMAKE_TOOLCHAIN_FILE expects ${sourceDir}/../vcpkg
VCPKG_ROOT="${VCPKG_ROOT:-$(cd "${REPO_ROOT}/.." && pwd)/vcpkg}"
QT_MACOS="${QT_ROOT}/${QT_VERSION}/macos"
QT6_DIR="${QT_MACOS}/lib/cmake/Qt6"

log "Repo:        ${REPO_ROOT}"
log "Qt root:     ${QT_ROOT} (version ${QT_VERSION})"
log "vcpkg root:  ${VCPKG_ROOT} (baseline ${VCPKG_BASELINE})"
log "Jobs:        ${JOBS}"

# --- 2) Homebrew build tools --------------------------------------------------
if [ "${SKIP_BREW:-0}" != "1" ]; then
  log "Installing Homebrew packages"
  brew install cmake ninja pkg-config nasm autoconf autoconf-archive
fi

hash -r
cmake --version | head -1
ninja --version

# --- 3) Qt SDK via aqtinstall -------------------------------------------------
# Prefer an on-disk SDK (actions/cache hit or prior local install).
if [ -d "${QT6_DIR}" ]; then
  log "Qt already present at ${QT_MACOS} (skipping aqtinstall)"
else
  log "Installing aqtinstall==${AQTINSTALL_VERSION}"
  python3 -m pip install --user --quiet "aqtinstall==${AQTINSTALL_VERSION}"

  log "Installing Qt ${QT_VERSION} (clang_64 + qtimageformats + qtpdf) into ${QT_ROOT}"
  python3 -m aqt install-qt mac desktop "${QT_VERSION}" clang_64 \
    --modules qtimageformats qtpdf \
    --outputdir "${QT_ROOT}"
fi

if [ ! -d "${QT6_DIR}" ]; then
  echo "ERROR: Qt6 cmake package not found at ${QT6_DIR}" >&2
  exit 1
fi
log "Using Qt6_DIR=${QT6_DIR}"

# --- 4) vcpkg at builtin-baseline (sibling of the repo) -----------------------
if [ ! -x "${VCPKG_ROOT}/vcpkg" ] || \
   [ "$(git -C "${VCPKG_ROOT}" rev-parse HEAD 2>/dev/null || true)" != "${VCPKG_BASELINE}" ]; then
  log "Fetching vcpkg ${VCPKG_BASELINE}"
  if [ ! -d "${VCPKG_ROOT}/.git" ]; then
    git clone --filter=blob:none https://github.com/microsoft/vcpkg.git "${VCPKG_ROOT}"
  fi
  git -C "${VCPKG_ROOT}" fetch --depth 1 origin "${VCPKG_BASELINE}"
  git -C "${VCPKG_ROOT}" checkout --force FETCH_HEAD
  log "Bootstrapping vcpkg"
  "${VCPKG_ROOT}/bootstrap-vcpkg.sh" -disableMetrics
else
  log "vcpkg already at ${VCPKG_BASELINE}"
fi

# --- 5) Configure + compile ---------------------------------------------------
cd "${REPO_ROOT}"

log "Configuring with preset MacOSNinjaRelease"
cmake -S "${REPO_ROOT}" -B "${REPO_ROOT}/build" \
  --preset MacOSNinjaRelease \
  -DQt6_DIR="${QT6_DIR}"

log "Building xSTUDIO"
cmake --build "${REPO_ROOT}/build" -j "${JOBS}"

log "macOS compile OK"
