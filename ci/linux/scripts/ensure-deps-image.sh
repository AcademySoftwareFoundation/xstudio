#!/usr/bin/env bash
# Ensure the Linux deps image is available locally (pull from GHCR, or build+push).
#
# Usage: ci/linux/scripts/ensure-deps-image.sh
#
# Required env:
#   DEPS_TAG     (ubuntu2404 | rocky9)
#   DEPS_FILE    (path from repo root, e.g. ci/linux/dockerfiles/ubuntu2404.deps.Dockerfile)
#   GITHUB_REPOSITORY (owner/repo) — used to form the GHCR image name
#
# Strategy env (from GitHub Actions):
#   GITHUB_EVENT_NAME
#   GITHUB_SHA
#   PR_HEAD_REPO   — github.event.pull_request.head.repo.full_name (PR only)
#   PR_BASE_SHA    — github.event.pull_request.base.sha (PR only)
#   PUSH_BEFORE    — github.event.before (push only)
#
# Optional:
#   JOBS=1
#   DEPS_IMAGE / DEPS_CACHE — override image tags
#   GITHUB_ENV / GITHUB_OUTPUT — when set, export DEPS_IMAGE for later steps

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
: "${DEPS_TAG:?DEPS_TAG is required}"
: "${DEPS_FILE:?DEPS_FILE is required}"
: "${GITHUB_REPOSITORY:?GITHUB_REPOSITORY is required}"

EVENT_NAME="${GITHUB_EVENT_NAME:-${EVENT_NAME:-}}"
: "${EVENT_NAME:?GITHUB_EVENT_NAME is required}"
SHA="${GITHUB_SHA:-${SHA:-}}"
: "${SHA:?GITHUB_SHA is required}"

repo_lc="$(printf '%s' "${GITHUB_REPOSITORY}" | tr '[:upper:]' '[:lower:]')"
DEPS_IMAGE="${DEPS_IMAGE:-ghcr.io/${repo_lc}/xstudio-deps:${DEPS_TAG}}"
DEPS_CACHE="${DEPS_CACHE:-ghcr.io/${repo_lc}/xstudio-deps:${DEPS_TAG}-buildcache}"
VERSIONS_ENV="${REPO_ROOT}/ci/linux/versions.env"
CONTEXT="${REPO_ROOT}/ci/linux"
DOCKERFILE="${REPO_ROOT}/${DEPS_FILE}"

if [ -n "${GITHUB_ENV:-}" ]; then
  {
    echo "DEPS_IMAGE=${DEPS_IMAGE}"
    echo "DEPS_CACHE=${DEPS_CACHE}"
  } >> "${GITHUB_ENV}"
fi

can_push=false
case "${EVENT_NAME}" in
  push|workflow_dispatch) can_push=true ;;
  pull_request)
    if [ "${PR_HEAD_REPO:-}" = "${GITHUB_REPOSITORY}" ]; then
      can_push=true
    fi
    ;;
esac

changed=false
case "${EVENT_NAME}" in
  workflow_dispatch)
    changed=true
    ;;
  pull_request)
    : "${PR_BASE_SHA:?PR_BASE_SHA is required for pull_request}"
    if git -C "${REPO_ROOT}" diff --name-only "${PR_BASE_SHA}" "${SHA}" | grep -qE '^ci/linux/'; then
      changed=true
    fi
    ;;
  push)
    zero=0000000000000000000000000000000000000000
    if [ -z "${PUSH_BEFORE:-}" ] || [ "${PUSH_BEFORE}" = "${zero}" ]; then
      changed=true
    elif git -C "${REPO_ROOT}" cat-file -e "${PUSH_BEFORE}^{commit}" 2>/dev/null \
      && git -C "${REPO_ROOT}" diff --name-only "${PUSH_BEFORE}" "${SHA}" | grep -qE '^ci/linux/'; then
      changed=true
    fi
    ;;
  *)
    echo "ERROR: unsupported GITHUB_EVENT_NAME=${EVENT_NAME}" >&2
    exit 1
    ;;
esac

echo "==> Deps strategy: can_push=${can_push} ci/linux changed=${changed}"
echo "==> Image: ${DEPS_IMAGE}"

need_build=false
if [ "${can_push}" = true ] && [ "${changed}" = true ]; then
  need_build=true
elif docker pull "${DEPS_IMAGE}"; then
  echo "==> Using existing ${DEPS_IMAGE}"
  exit 0
elif [ "${can_push}" = true ]; then
  echo "==> Deps image missing; will build and push"
  need_build=true
else
  echo "ERROR: deps image ${DEPS_IMAGE} is not available and this job cannot push to GHCR" >&2
  echo "  Fork PRs reuse the base repo image; publish it from main/develop or a same-repo PR that touches ci/linux/." >&2
  exit 1
fi

[ "${need_build}" = true ]

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

echo "==> Building and pushing ${DEPS_IMAGE}"
docker buildx build \
  --push \
  --tag "${DEPS_IMAGE}" \
  --cache-from "type=registry,ref=${DEPS_CACHE}" \
  --cache-to "type=registry,ref=${DEPS_CACHE},mode=max" \
  "${build_args[@]}" \
  -f "${DOCKERFILE}" \
  "${CONTEXT}"
