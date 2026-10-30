#!/usr/bin/env bash
# Build xSTUDIO inside the toolchain image: vcpkg install (NuGet) + cmake build.
# sccache directory is host-mounted so the compiler cache survives commits.
#
# Usage: ci/linux/scripts/run-build-container.sh
#
# Required env:
#   DEPS_IMAGE   (toolchain image, e.g. ghcr.io/.../xstudio-toolchain:ubuntu2404-<hash>)
#   DEPS_TAG     (ubuntu2404 | rocky9)
#   VCPKG_ROOT   (host path; mounted as sibling of the repo inside the container)
#
# Optional:
#   SCCACHE_DIR / SCCACHE_BIN_DIR
#   JOBS, BUILD_TESTING, BMD_DECKLINK, STUDIO_PLUGINS
#   VCPKG_BINARY_SOURCES, GITHUB_TOKEN / NUGET_PASSWORD, FEED_URL, NUGET_OWNER

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
: "${DEPS_IMAGE:?DEPS_IMAGE is required}"
: "${DEPS_TAG:?DEPS_TAG is required}"
: "${VCPKG_ROOT:?VCPKG_ROOT is required}"

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
    VCPKG_TARGET_TRIPLET="${VCPKG_TARGET_TRIPLET:-x64-xstudio-linux-ubuntu2404}"
    ;;
  rocky9)
    VCPKG_TARGET_TRIPLET="${VCPKG_TARGET_TRIPLET:-x64-xstudio-linux-rocky9}"
    ;;
  *)
    echo "ERROR: unknown DEPS_TAG=${DEPS_TAG}" >&2
    exit 1
    ;;
esac

if docker image inspect "${DEPS_IMAGE}" >/dev/null 2>&1; then
  echo "==> Using local image ${DEPS_IMAGE}"
else
  echo "==> Pulling ${DEPS_IMAGE}"
  docker pull "${DEPS_IMAGE}"
fi

if [ ! -d "${VCPKG_ROOT}" ]; then
  echo "ERROR: VCPKG_ROOT does not exist: ${VCPKG_ROOT}" >&2
  exit 1
fi

# Sibling layout inside the container for CMakePresets toolchain path.
CONTAINER_SRC=/src/xstudio
CONTAINER_VCPKG=/src/vcpkg

HOST_UID="$(id -u)"
HOST_GID="$(id -g)"

echo "==> Building xSTUDIO in container (triplet=${VCPKG_TARGET_TRIPLET}, sccache=${SCCACHE_DIR})"
docker run --rm \
  -v "${REPO_ROOT}:${CONTAINER_SRC}" \
  -v "${VCPKG_ROOT}:${CONTAINER_VCPKG}" \
  -v "${SCCACHE_DIR}:/sccache" \
  -v "${SCCACHE_BIN_DIR}:/sccache-bin:ro" \
  -e SCCACHE_DIR=/sccache \
  -e CMAKE_C_COMPILER_LAUNCHER=/sccache-bin/sccache \
  -e CMAKE_CXX_COMPILER_LAUNCHER=/sccache-bin/sccache \
  -e JOBS="${JOBS:-1}" \
  -e BUILD_TESTING="${BUILD_TESTING:-OFF}" \
  -e BMD_DECKLINK="${BMD_DECKLINK:-OFF}" \
  -e STUDIO_PLUGINS="${STUDIO_PLUGINS:-}" \
  -e QT_ROOT=/opt/qt \
  -e XSTUDIO_SRC="${CONTAINER_SRC}" \
  -e VCPKG_ROOT="${CONTAINER_VCPKG}" \
  -e VCPKG_TARGET_TRIPLET="${VCPKG_TARGET_TRIPLET}" \
  -e VCPKG_BINARY_SOURCES="${VCPKG_BINARY_SOURCES:-}" \
  -e FEED_URL="${FEED_URL:-}" \
  -e NUGET_OWNER="${NUGET_OWNER:-}" \
  -e NUGET_PASSWORD="${NUGET_PASSWORD:-${GITHUB_TOKEN:-}}" \
  -e GITHUB_TOKEN="${GITHUB_TOKEN:-}" \
  -e SKIP_VCPKG_BOOTSTRAP="${SKIP_VCPKG_BOOTSTRAP:-0}" \
  "${DEPS_IMAGE}" \
  bash -lc "\
    set +e; \
    bash ${CONTAINER_SRC}/ci/linux/scripts/build-deps.sh; \
    status=\$?; \
    if [ \"\$status\" -eq 0 ]; then \
      bash ${CONTAINER_SRC}/ci/linux/scripts/build-xstudio.sh; \
      status=\$?; \
    fi; \
    /sccache-bin/sccache --show-stats > ${CONTAINER_SRC}/.ci-cache/sccache-show-stats.txt 2>&1 || true; \
    chown -R ${HOST_UID}:${HOST_GID} /sccache ${CONTAINER_SRC}/.ci-cache/sccache-show-stats.txt ${CONTAINER_SRC}/build ${CONTAINER_VCPKG} 2>/dev/null || true; \
    exit \$status"
