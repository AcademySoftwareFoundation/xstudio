#!/usr/bin/env bash
# Ensure the Linux toolchain image is available locally (pull from GHCR, or build).
#
# Usage: ci/linux/scripts/ensure-toolchain-image.sh
#
# Required env:
#   DEPS_TAG     (ubuntu2404 | rocky9)
#   DEPS_FILE    (path from repo root, e.g. ci/linux/dockerfiles/ubuntu2404.toolchain.Dockerfile)
#   GITHUB_REPOSITORY (owner/repo)
#
# Strategy env: GITHUB_EVENT_NAME, PR_HEAD_REPO (PR only)
#
# Optional: JOBS, TOOLCHAIN_IMAGE / TOOLCHAIN_CACHE / TOOLCHAIN_CONTENT_HASH, GITHUB_ENV
#
# Tag: ghcr.io/<repo>/xstudio-toolchain:<distro>-<hash12>
# Hash inputs: versions.env, *toolchain*.Dockerfile, build-common.sh, build-tools.sh
# Pull order: primary → fork → build (--push if can_push else --load)

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
: "${DEPS_TAG:?DEPS_TAG is required}"
: "${DEPS_FILE:?DEPS_FILE is required}"
: "${GITHUB_REPOSITORY:?GITHUB_REPOSITORY is required}"

EVENT_NAME="${GITHUB_EVENT_NAME:-${EVENT_NAME:-}}"
: "${EVENT_NAME:?GITHUB_EVENT_NAME is required}"

toolchain_content_hash() {
  local list_file manifest_file
  list_file="$(mktemp)"
  manifest_file="$(mktemp)"
  # shellcheck disable=SC2064
  trap "rm -f '${list_file}' '${manifest_file}'" RETURN

  (
    cd "${REPO_ROOT}"
    printf '%s\n' \
      ci/linux/versions.env \
      ci/linux/scripts/build-common.sh \
      ci/linux/scripts/build-tools.sh
    shopt -s nullglob
    local dockerfiles=(ci/linux/dockerfiles/*toolchain*.Dockerfile)
    if [ "${#dockerfiles[@]}" -eq 0 ]; then
      echo "ERROR: no ci/linux/dockerfiles/*toolchain*.Dockerfile found" >&2
      exit 1
    fi
    printf '%s\n' "${dockerfiles[@]}"
  ) | LC_ALL=C sort -u > "${list_file}"

  local rel path
  while IFS= read -r rel; do
    path="${REPO_ROOT}/${rel}"
    if [ ! -f "${path}" ]; then
      echo "ERROR: toolchain content-hash input missing: ${rel}" >&2
      exit 1
    fi
    printf '%s  %s\n' "$(sha256sum "${path}" | awk '{print $1}')" "${rel}"
  done < "${list_file}" > "${manifest_file}"

  sha256sum "${manifest_file}" | awk '{print substr($1, 1, 12)}'
}

TOOLCHAIN_CONTENT_HASH="${TOOLCHAIN_CONTENT_HASH:-$(toolchain_content_hash)}"
IMAGE_TAG="${DEPS_TAG}-${TOOLCHAIN_CONTENT_HASH}"

repo_lc="$(printf '%s' "${GITHUB_REPOSITORY}" | tr '[:upper:]' '[:lower:]')"
TOOLCHAIN_IMAGE="${TOOLCHAIN_IMAGE:-ghcr.io/${repo_lc}/xstudio-toolchain:${IMAGE_TAG}}"
TOOLCHAIN_CACHE="${TOOLCHAIN_CACHE:-ghcr.io/${repo_lc}/xstudio-toolchain:${IMAGE_TAG}-buildcache}"
DEPS_IMAGE="${DEPS_IMAGE:-${TOOLCHAIN_IMAGE}}"
VERSIONS_ENV="${REPO_ROOT}/ci/linux/versions.env"
CONTEXT="${REPO_ROOT}/ci/linux"
DOCKERFILE="${REPO_ROOT}/${DEPS_FILE}"

if [ -n "${GITHUB_ENV:-}" ]; then
  {
    echo "TOOLCHAIN_IMAGE=${TOOLCHAIN_IMAGE}"
    echo "TOOLCHAIN_CACHE=${TOOLCHAIN_CACHE}"
    echo "TOOLCHAIN_CONTENT_HASH=${TOOLCHAIN_CONTENT_HASH}"
    echo "DEPS_IMAGE=${DEPS_IMAGE}"
  } >> "${GITHUB_ENV}"
fi

FORK_IMAGE=""
FORK_CACHE=""
if [ -n "${PR_HEAD_REPO:-}" ] && [ "${PR_HEAD_REPO}" != "${GITHUB_REPOSITORY}" ]; then
  fork_lc="$(printf '%s' "${PR_HEAD_REPO}" | tr '[:upper:]' '[:lower:]')"
  FORK_IMAGE="ghcr.io/${fork_lc}/xstudio-toolchain:${IMAGE_TAG}"
  FORK_CACHE="ghcr.io/${fork_lc}/xstudio-toolchain:${IMAGE_TAG}-buildcache"
fi

can_push=false
case "${EVENT_NAME}" in
  push|workflow_dispatch) can_push=true ;;
  pull_request)
    if [ "${PR_HEAD_REPO:-}" = "${GITHUB_REPOSITORY}" ]; then
      can_push=true
    fi
    ;;
  *)
    echo "ERROR: unsupported GITHUB_EVENT_NAME=${EVENT_NAME}" >&2
    exit 1
    ;;
esac

echo "==> Toolchain content hash: ${TOOLCHAIN_CONTENT_HASH}"
echo "==> Toolchain strategy: can_push=${can_push}"
echo "==> Primary image: ${TOOLCHAIN_IMAGE}"
[ -n "${FORK_IMAGE}" ] && echo "==> Fork fallback: ${FORK_IMAGE}"

pull_toolchain_image() {
  if docker pull "${TOOLCHAIN_IMAGE}"; then
    echo "==> Using ${TOOLCHAIN_IMAGE}"
    return 0
  fi
  if [ -n "${FORK_IMAGE}" ]; then
    echo "==> Primary miss; trying fork image ${FORK_IMAGE}"
    if docker pull "${FORK_IMAGE}"; then
      docker tag "${FORK_IMAGE}" "${TOOLCHAIN_IMAGE}"
      echo "==> Using fork image (tagged as ${TOOLCHAIN_IMAGE})"
      return 0
    fi
  fi
  return 1
}

if pull_toolchain_image; then
  exit 0
fi

echo "==> No reusable image; will build$([ "${can_push}" = true ] && echo ' and push' || echo ' locally (no GHCR push)')"

if [ ! -f "${DOCKERFILE}" ]; then
  echo "ERROR: Dockerfile not found: ${DOCKERFILE}" >&2
  exit 1
fi
if [ ! -f "${VERSIONS_ENV}" ]; then
  echo "ERROR: versions.env not found: ${VERSIONS_ENV}" >&2
  exit 1
fi

build_args=(--build-arg "JOBS=${JOBS:-1}")
while IFS= read -r line || [ -n "${line}" ]; do
  case "${line}" in
    ''|\#*) continue ;;
  esac
  build_args+=(--build-arg "${line}")
done < "${VERSIONS_ENV}"

common=(
  --tag "${TOOLCHAIN_IMAGE}"
  --cache-from "type=registry,ref=${TOOLCHAIN_CACHE}"
  "${build_args[@]}"
  -f "${DOCKERFILE}"
  "${CONTEXT}"
)

if [ -n "${FORK_CACHE}" ]; then
  common+=(--cache-from "type=registry,ref=${FORK_CACHE}")
fi

if [ "${can_push}" = true ]; then
  echo "==> Building and pushing ${TOOLCHAIN_IMAGE}"
  docker buildx build \
    --push \
    --cache-to "type=registry,ref=${TOOLCHAIN_CACHE},mode=max" \
    "${common[@]}"
else
  echo "==> Building locally (no push) ${TOOLCHAIN_IMAGE}"
  docker buildx build \
    --load \
    "${common[@]}"
fi
