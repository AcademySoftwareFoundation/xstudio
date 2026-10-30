#!/usr/bin/env bash
# Ensure the Linux deps image is available locally (pull from GHCR, or build).
#
# Usage: ci/linux/scripts/ensure-deps-image.sh
#
# Required env:
#   DEPS_TAG     (ubuntu2404 | rocky9)
#   DEPS_FILE    (path from repo root, e.g. ci/linux/dockerfiles/ubuntu2404.deps.Dockerfile)
#   GITHUB_REPOSITORY (owner/repo) — primary GHCR image (base repo on PRs)
#
# Strategy env (from GitHub Actions):
#   GITHUB_EVENT_NAME
#   PR_HEAD_REPO   — github.event.pull_request.head.repo.full_name (PR only)
#
# Optional:
#   JOBS=1
#   DEPS_IMAGE / DEPS_CACHE — override primary image tags
#   DEPS_CONTENT_HASH — override content hash used in default tags
#   GITHUB_ENV — when set, export DEPS_IMAGE for later steps
#
# Image tag is content-addressed:
#   ghcr.io/<repo>/xstudio-deps:<distro>-<hash12>
# where hash12 covers only deps inputs (versions.env, *deps* Dockerfiles,
# build-common/tools, deps/*.sh) — see deps-content-hash.sh.
#
# Pull order:
#   1) primary: ghcr.io/<GITHUB_REPOSITORY>/xstudio-deps:<distro>-<hash>
#   2) fallback: ghcr.io/<PR_HEAD_REPO>/… same tag (fork), retagged locally
#   3) build (push if same-repo can write packages; else --load only)
#
# Note: fork fallback only works if that GHCR package is pullable by the runner
# (public package, or otherwise authorized).

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
: "${DEPS_TAG:?DEPS_TAG is required}"
: "${DEPS_FILE:?DEPS_FILE is required}"
: "${GITHUB_REPOSITORY:?GITHUB_REPOSITORY is required}"

EVENT_NAME="${GITHUB_EVENT_NAME:-${EVENT_NAME:-}}"
: "${EVENT_NAME:?GITHUB_EVENT_NAME is required}"

DEPS_CONTENT_HASH="${DEPS_CONTENT_HASH:-$(bash "${REPO_ROOT}/ci/linux/scripts/deps-content-hash.sh")}"
IMAGE_TAG="${DEPS_TAG}-${DEPS_CONTENT_HASH}"

repo_lc="$(printf '%s' "${GITHUB_REPOSITORY}" | tr '[:upper:]' '[:lower:]')"
DEPS_IMAGE="${DEPS_IMAGE:-ghcr.io/${repo_lc}/xstudio-deps:${IMAGE_TAG}}"
DEPS_CACHE="${DEPS_CACHE:-ghcr.io/${repo_lc}/xstudio-deps:${IMAGE_TAG}-buildcache}"
VERSIONS_ENV="${REPO_ROOT}/ci/linux/versions.env"
CONTEXT="${REPO_ROOT}/ci/linux"
DOCKERFILE="${REPO_ROOT}/${DEPS_FILE}"

if [ -n "${GITHUB_ENV:-}" ]; then
  {
    echo "DEPS_IMAGE=${DEPS_IMAGE}"
    echo "DEPS_CACHE=${DEPS_CACHE}"
    echo "DEPS_CONTENT_HASH=${DEPS_CONTENT_HASH}"
  } >> "${GITHUB_ENV}"
fi

# Secondary image from the PR head fork (when different from the base repo).
FORK_IMAGE=""
FORK_CACHE=""
if [ -n "${PR_HEAD_REPO:-}" ] && [ "${PR_HEAD_REPO}" != "${GITHUB_REPOSITORY}" ]; then
  fork_lc="$(printf '%s' "${PR_HEAD_REPO}" | tr '[:upper:]' '[:lower:]')"
  FORK_IMAGE="ghcr.io/${fork_lc}/xstudio-deps:${IMAGE_TAG}"
  FORK_CACHE="ghcr.io/${fork_lc}/xstudio-deps:${IMAGE_TAG}-buildcache"
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

echo "==> Deps content hash: ${DEPS_CONTENT_HASH}"
echo "==> Deps strategy: can_push=${can_push}"
echo "==> Primary image: ${DEPS_IMAGE}"
[ -n "${FORK_IMAGE}" ] && echo "==> Fork fallback: ${FORK_IMAGE}"

# Try primary (ASWF / base repo), then fork. On success, tag as DEPS_IMAGE for later steps.
pull_deps_image() {
  if docker pull "${DEPS_IMAGE}"; then
    echo "==> Using ${DEPS_IMAGE}"
    return 0
  fi
  if [ -n "${FORK_IMAGE}" ]; then
    echo "==> Primary miss; trying fork image ${FORK_IMAGE}"
    if docker pull "${FORK_IMAGE}"; then
      docker tag "${FORK_IMAGE}" "${DEPS_IMAGE}"
      echo "==> Using fork image (tagged as ${DEPS_IMAGE})"
      return 0
    fi
  fi
  return 1
}

if pull_deps_image; then
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
  --tag "${DEPS_IMAGE}"
  --cache-from "type=registry,ref=${DEPS_CACHE}"
  "${build_args[@]}"
  -f "${DOCKERFILE}"
  "${CONTEXT}"
)

# Also try fork buildcache as cache source when present.
if [ -n "${FORK_CACHE}" ]; then
  common+=(--cache-from "type=registry,ref=${FORK_CACHE}")
fi

if [ "${can_push}" = true ]; then
  echo "==> Building and pushing ${DEPS_IMAGE}"
  docker buildx build \
    --push \
    --cache-to "type=registry,ref=${DEPS_CACHE},mode=max" \
    "${common[@]}"
else
  echo "==> Building locally (no push) ${DEPS_IMAGE}"
  docker buildx build \
    --load \
    "${common[@]}"
fi
