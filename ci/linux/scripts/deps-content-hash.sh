#!/usr/bin/env bash
# Compute a deterministic content hash of the inputs that define the Linux deps image.
#
# Usage:
#   hash="$(bash ci/linux/scripts/deps-content-hash.sh)"
#
# Hashed inputs (sorted, relative to repo root):
#   ci/linux/versions.env
#   ci/linux/dockerfiles/*deps*.Dockerfile
#   ci/linux/scripts/build-common.sh
#   ci/linux/scripts/build-tools.sh
#   ci/linux/scripts/deps/*.sh
#
# Excluded (do not change the deps image): build-xstudio.sh, run-tests.sh,
# run-build-container.sh, ensure-deps-image.sh, *build*.Dockerfile, etc.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
LIST_FILE="$(mktemp)"
MANIFEST_FILE="$(mktemp)"
trap 'rm -f "${LIST_FILE}" "${MANIFEST_FILE}"' EXIT

(
  cd "${REPO_ROOT}"
  printf '%s\n' \
    ci/linux/versions.env \
    ci/linux/scripts/build-common.sh \
    ci/linux/scripts/build-tools.sh
  # Globs expand in the repo; fail if either set is empty.
  shopt -s nullglob
  deps_dockerfiles=(ci/linux/dockerfiles/*deps*.Dockerfile)
  deps_scripts=(ci/linux/scripts/deps/*.sh)
  if [ "${#deps_dockerfiles[@]}" -eq 0 ]; then
    echo "ERROR: no ci/linux/dockerfiles/*deps*.Dockerfile found" >&2
    exit 1
  fi
  if [ "${#deps_scripts[@]}" -eq 0 ]; then
    echo "ERROR: no ci/linux/scripts/deps/*.sh found" >&2
    exit 1
  fi
  printf '%s\n' "${deps_dockerfiles[@]}"
  printf '%s\n' "${deps_scripts[@]}"
) | LC_ALL=C sort -u > "${LIST_FILE}"

while IFS= read -r rel; do
  path="${REPO_ROOT}/${rel}"
  if [ ! -f "${path}" ]; then
    echo "ERROR: deps content-hash input missing: ${rel}" >&2
    exit 1
  fi
  # path-relative + content sha256 (stable across machines).
  printf '%s  %s\n' "$(sha256sum "${path}" | awk '{print $1}')" "${rel}"
done < "${LIST_FILE}" > "${MANIFEST_FILE}"

# First 12 hex chars of the manifest digest.
sha256sum "${MANIFEST_FILE}" | awk '{print substr($1, 1, 12)}'
