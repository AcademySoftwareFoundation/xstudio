#!/usr/bin/env bash
# Install macOS build deps: Homebrew tools, Qt SDK, vcpkg + manifest ports.
#
# Usage: ci/macos/scripts/build-deps.sh
#
# Overridable env:
#   JOBS=1
#   QT_ROOT=$HOME/Qt
#   VCPKG_ROOT=<sibling-of-repo>/vcpkg
#   SKIP_BREW=1

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MACOS_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
REPO_ROOT="$(cd "${MACOS_DIR}/../.." && pwd)"

log() { printf '\n\033[1;34m==> %s\033[0m\n' "$*"; }

# shellcheck source=../versions.env
source "${MACOS_DIR}/versions.env"
: "${QT_VERSION:?QT_VERSION missing from ci/macos/versions.env}"
: "${AQTINSTALL_VERSION:?AQTINSTALL_VERSION missing from ci/macos/versions.env}"

VCPKG_BASELINE="$(
  python3 -c "import json,sys; print(json.load(open(sys.argv[1]))['builtin-baseline'])" \
    "${REPO_ROOT}/vcpkg.json"
)"
: "${VCPKG_BASELINE:?could not read builtin-baseline from vcpkg.json}"

export JOBS="${JOBS:-1}"
QT_ROOT="${QT_ROOT:-${HOME}/Qt}"
VCPKG_ROOT="${VCPKG_ROOT:-$(cd "${REPO_ROOT}/.." && pwd)/vcpkg}"
QT_MACOS="${QT_ROOT}/${QT_VERSION}/macos"
QT6_DIR="${QT_MACOS}/lib/cmake/Qt6"

log "Repo:       ${REPO_ROOT}"
log "Qt root:    ${QT_ROOT} (version ${QT_VERSION})"
log "vcpkg root: ${VCPKG_ROOT} (baseline ${VCPKG_BASELINE})"
log "Jobs:       ${JOBS}"

# --- Homebrew -----------------------------------------------------------------
if [ "${SKIP_BREW:-0}" != "1" ]; then
  log "Installing Homebrew packages"
  brew install cmake ninja pkg-config nasm autoconf automake autoconf-archive
fi
hash -r
cmake --version | head -1
ninja --version

# --- Qt SDK -------------------------------------------------------------------
if [ -d "${QT6_DIR}" ]; then
  log "Qt already present at ${QT_MACOS} (skipping aqtinstall)"
else
  AQT_VENV="${REPO_ROOT}/.ci-cache/aqt-venv"
  log "Installing aqtinstall==${AQTINSTALL_VERSION} into ${AQT_VENV}"
  mkdir -p "$(dirname "${AQT_VENV}")"
  python3 -m venv "${AQT_VENV}"
  # shellcheck disable=SC1091
  source "${AQT_VENV}/bin/activate"
  python3 -m pip install --quiet "aqtinstall==${AQTINSTALL_VERSION}"

  log "Installing Qt ${QT_VERSION} (clang_64 + qtimageformats + qtpdf) into ${QT_ROOT}"
  python3 -m aqt install-qt mac desktop "${QT_VERSION}" clang_64 \
    --modules qtimageformats qtpdf \
    --outputdir "${QT_ROOT}"
  deactivate
fi

if [ ! -d "${QT6_DIR}" ]; then
  echo "ERROR: Qt6 cmake package not found at ${QT6_DIR}" >&2
  exit 1
fi
log "Using Qt6_DIR=${QT6_DIR}"

# --- vcpkg bootstrap (skipped when CI already bootstrapped this baseline) ------
if [ ! -x "${VCPKG_ROOT}/vcpkg" ] || \
   [ "$(git -C "${VCPKG_ROOT}" rev-parse HEAD 2>/dev/null || true)" != "${VCPKG_BASELINE}" ]; then
  log "Fetching vcpkg ${VCPKG_BASELINE}"
  if [ ! -d "${VCPKG_ROOT}/.git" ]; then
    git clone https://github.com/microsoft/vcpkg.git "${VCPKG_ROOT}"
  else
    git -C "${VCPKG_ROOT}" fetch origin
  fi
  git -C "${VCPKG_ROOT}" checkout --force "${VCPKG_BASELINE}"
  log "Bootstrapping vcpkg"
  "${VCPKG_ROOT}/bootstrap-vcpkg.sh" -disableMetrics
else
  log "vcpkg already at ${VCPKG_BASELINE} (skip bootstrap)"
fi

# --- vcpkg manifest ports (ffmpeg, OIIO, …) -----------------------------------
# Same install root as build-xstudio.sh (-DVCPKG_INSTALLED_DIR=.../vcpkg_installed).
log "Installing vcpkg manifest dependencies (triplet arm-osx)"
(
  cd "${REPO_ROOT}"
  "${VCPKG_ROOT}/vcpkg" install \
    --triplet arm-osx \
    --overlay-ports="${REPO_ROOT}/cmake/vcpkg_overlay_ports" \
    --overlay-triplets="${REPO_ROOT}/cmake/vcpkg_triplets" \
    --x-install-root="${REPO_ROOT}/vcpkg_installed"
)

log "macOS deps OK"
