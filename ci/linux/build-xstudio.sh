#!/usr/bin/env bash
# Build xSTUDIO against an existing deps image (local / VS Code).
# Usage: ci/linux/build-xstudio.sh ubuntu24|rocky9
# Requires: deps image already built (ci/linux/build-deps.sh).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${ROOT}/../.." && pwd)"

distro="${1:?usage: $0 ubuntu24|rocky9}"
case "$distro" in
  ubuntu24)
    df="${ROOT}/dockerfiles/ubuntu2404.build.Dockerfile"
    deps_img="xstudio-deps:ubuntu2404"
    label="Ubuntu 24.04"
    ;;
  rocky9)
    df="${ROOT}/dockerfiles/rocky9.build.Dockerfile"
    deps_img="xstudio-deps:rocky9"
    label="Rocky 9"
    ;;
  *)
    echo "ERROR: unknown distro '$distro' (want ubuntu24|rocky9)" >&2
    exit 1
    ;;
esac

export DOCKER_BUILDKIT=1
docker build \
  -f "$df" \
  --target build \
  --build-arg "JOBS=${JOBS:-8}" \
  --build-arg "BMD_DECKLINK=${BMD_DECKLINK:-OFF}" \
  --build-arg "STUDIO_PLUGINS=${STUDIO_PLUGINS:-}" \
  --build-arg "DEPS_IMAGE=${DEPS_IMAGE:-$deps_img}" \
  "$REPO_ROOT"

echo "==> ${label} build OK"
