#!/usr/bin/env bash
# Run a Linux CI script inside the toolchain image (deps and/or xSTUDIO build).
# sccache is host-mounted so the compiler cache survives commits.
#
# Usage:
#   ci/linux/scripts/run-build-container.sh deps
#   ci/linux/scripts/run-build-container.sh build
#
# Required env:
#   DEPS_IMAGE   (toolchain image, e.g. ghcr.io/.../xstudio-toolchain:ubuntu2404-<hash>)
#   DEPS_TAG     (ubuntu2404 | rocky9)
#   VCPKG_ROOT   (host path; mounted as sibling of the repo inside the container)
#
# Optional:
#   SCCACHE_DIR / SCCACHE_BIN_DIR  (required for "build")
#   JOBS, BUILD_TESTING, BMD_DECKLINK, STUDIO_PLUGINS
#   VCPKG_BINARY_SOURCES, GITHUB_TOKEN / NUGET_PASSWORD, FEED_URL, NUGET_OWNER
#   SKIP_VCPKG_BOOTSTRAP

set -euo pipefail

MODE="${1:?usage: run-build-container.sh deps|build}"

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
: "${DEPS_IMAGE:?DEPS_IMAGE is required}"
: "${DEPS_TAG:?DEPS_TAG is required}"
: "${VCPKG_ROOT:?VCPKG_ROOT is required}"

SCCACHE_DIR="${SCCACHE_DIR:-${REPO_ROOT}/.ci-cache/sccache}"
SCCACHE_BIN_DIR="${SCCACHE_BIN_DIR:-${REPO_ROOT}/.ci-cache/sccache-bin}"

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

CONTAINER_SRC=/src/xstudio
CONTAINER_VCPKG=/src/vcpkg
HOST_UID="$(id -u)"
HOST_GID="$(id -g)"

docker_env=(
  -e "JOBS=${JOBS:-1}"
  -e "QT_ROOT=/opt/qt"
  -e "XSTUDIO_SRC=${CONTAINER_SRC}"
  -e "VCPKG_ROOT=${CONTAINER_VCPKG}"
  -e "VCPKG_TARGET_TRIPLET=${VCPKG_TARGET_TRIPLET}"
  -e "VCPKG_BINARY_SOURCES=${VCPKG_BINARY_SOURCES:-}"
  -e "FEED_URL=${FEED_URL:-}"
  -e "NUGET_OWNER=${NUGET_OWNER:-}"
  -e "NUGET_PASSWORD=${NUGET_PASSWORD:-${GITHUB_TOKEN:-}}"
  -e "GITHUB_TOKEN=${GITHUB_TOKEN:-}"
  -e "SKIP_VCPKG_BOOTSTRAP=${SKIP_VCPKG_BOOTSTRAP:-0}"
)

docker_vols=(
  -v "${REPO_ROOT}:${CONTAINER_SRC}"
  -v "${VCPKG_ROOT}:${CONTAINER_VCPKG}"
)

case "${MODE}" in
  deps)
    echo "==> Build deps in container (triplet=${VCPKG_TARGET_TRIPLET})"
    inner="bash ${CONTAINER_SRC}/ci/linux/scripts/build-deps.sh; status=\$?; \
      chown -R ${HOST_UID}:${HOST_GID} ${CONTAINER_SRC}/vcpkg_installed ${CONTAINER_VCPKG} 2>/dev/null || true; \
      exit \$status"
    ;;
  build)
    mkdir -p "${SCCACHE_DIR}"
    if [ ! -x "${SCCACHE_BIN_DIR}/sccache" ]; then
      echo "ERROR: sccache binary not found at ${SCCACHE_BIN_DIR}/sccache" >&2
      echo "  Run ci/linux/scripts/install-sccache.sh first." >&2
      exit 1
    fi
    docker_vols+=(
      -v "${SCCACHE_DIR}:/sccache"
      -v "${SCCACHE_BIN_DIR}:/sccache-bin:ro"
    )
    docker_env+=(
      -e SCCACHE_DIR=/sccache
      -e CMAKE_C_COMPILER_LAUNCHER=/sccache-bin/sccache
      -e CMAKE_CXX_COMPILER_LAUNCHER=/sccache-bin/sccache
      -e "BUILD_TESTING=${BUILD_TESTING:-OFF}"
      -e "BMD_DECKLINK=${BMD_DECKLINK:-OFF}"
      -e "STUDIO_PLUGINS=${STUDIO_PLUGINS:-}"
    )
    echo "==> Build xSTUDIO in container (triplet=${VCPKG_TARGET_TRIPLET}, sccache=${SCCACHE_DIR})"
    inner="bash ${CONTAINER_SRC}/ci/linux/scripts/build-xstudio.sh; status=\$?; \
      /sccache-bin/sccache --show-stats > ${CONTAINER_SRC}/.ci-cache/sccache-show-stats.txt 2>&1 || true; \
      chown -R ${HOST_UID}:${HOST_GID} /sccache ${CONTAINER_SRC}/.ci-cache/sccache-show-stats.txt ${CONTAINER_SRC}/build 2>/dev/null || true; \
      exit \$status"
    ;;
  *)
    echo "ERROR: unknown mode '${MODE}' (expected deps|build)" >&2
    exit 1
    ;;
esac

docker run --rm \
  "${docker_vols[@]}" \
  "${docker_env[@]}" \
  "${DEPS_IMAGE}" \
  bash -lc "set +e; ${inner}"
