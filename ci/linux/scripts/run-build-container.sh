#!/usr/bin/env bash
# Build xSTUDIO inside the deps image with a host-mounted sccache directory.
# Used by CI so compiler cache survives across commits on the same branch.
#
# Usage: ci/linux/scripts/run-build-container.sh
#
# Required env:
#   DEPS_IMAGE   (e.g. ghcr.io/.../xstudio-deps:ubuntu2404)
#   DEPS_TAG     (ubuntu2404 | rocky9) — selects package install flavour
# Optional:
#   SCCACHE_DIR=.ci-cache/sccache
#   SCCACHE_BIN_DIR=.ci-cache/sccache-bin  (must contain ./sccache)
#   JOBS, BUILD_TESTING, BMD_DECKLINK, STUDIO_PLUGINS

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
: "${DEPS_IMAGE:?DEPS_IMAGE is required}"
: "${DEPS_TAG:?DEPS_TAG is required}"

SCCACHE_DIR="${SCCACHE_DIR:-${REPO_ROOT}/.ci-cache/sccache}"
SCCACHE_BIN_DIR="${SCCACHE_BIN_DIR:-${REPO_ROOT}/.ci-cache/sccache-bin}"
mkdir -p "${SCCACHE_DIR}"

if [ ! -x "${SCCACHE_BIN_DIR}/sccache" ]; then
  echo "ERROR: sccache binary not found at ${SCCACHE_BIN_DIR}/sccache" >&2
  echo "  Run ci/scripts/install-sccache.sh first." >&2
  exit 1
fi

case "${DEPS_TAG}" in
  ubuntu2404)
    pkg_setup='apt-get update && apt-get install -y --no-install-recommends libxkbcommon-dev libfontconfig-dev libgtest-dev'
    ;;
  rocky9)
    pkg_setup='dnf -y install epel-release && dnf -y install libxkbcommon-devel gtest-devel && dnf clean all'
    ;;
  *)
    echo "ERROR: unknown DEPS_TAG=${DEPS_TAG}" >&2
    exit 1
    ;;
esac

echo "==> Pulling ${DEPS_IMAGE}"
docker pull "${DEPS_IMAGE}"

echo "==> Building xSTUDIO in container (sccache dir: ${SCCACHE_DIR})"
docker run --rm \
  -v "${REPO_ROOT}:/src/xstudio" \
  -v "${SCCACHE_DIR}:/sccache" \
  -v "${SCCACHE_BIN_DIR}:/sccache-bin:ro" \
  -e SCCACHE_DIR=/sccache \
  -e CMAKE_C_COMPILER_LAUNCHER=/sccache-bin/sccache \
  -e CMAKE_CXX_COMPILER_LAUNCHER=/sccache-bin/sccache \
  -e JOBS="${JOBS:-1}" \
  -e BUILD_TESTING="${BUILD_TESTING:-OFF}" \
  -e BMD_DECKLINK="${BMD_DECKLINK:-OFF}" \
  -e STUDIO_PLUGINS="${STUDIO_PLUGINS:-}" \
  -e QT_VERSION \
  -e QT_ROOT=/opt/qt \
  -e XSTUDIO_SRC=/src/xstudio \
  "${DEPS_IMAGE}" \
  bash -lc "${pkg_setup} && bash /src/xstudio/ci/linux/scripts/build-xstudio.sh"

echo "==> sccache stats"
SCCACHE_DIR="${SCCACHE_DIR}" "${SCCACHE_BIN_DIR}/sccache" --show-stats
