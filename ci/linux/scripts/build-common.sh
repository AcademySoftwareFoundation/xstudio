#!/usr/bin/env bash
# Shared helpers for Linux CI scripts (sourced, not executed).
# Provides: JOBS, log, source_versions_env, require_env

set -euo pipefail

export JOBS="${JOBS:-1}"

log() { printf '\n\033[1;34m==> %s\033[0m\n' "$*"; }

# Load ci/linux/versions.env when present (local runs; no-op if pins already in ENV).
source_versions_env() {
  local versions
  versions="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)/versions.env"
  if [ -f "$versions" ]; then
    # shellcheck source=../versions.env
    source "$versions"
  fi
}

# Fail early if a required pin/env var is missing.
require_env() {
  local missing=0 v
  for v in "$@"; do
    if [ -z "${!v:-}" ]; then
      echo "ERROR: required '$v' is not set (check versions.env / Docker ENV)" >&2
      missing=1
    fi
  done
  [ "$missing" -eq 0 ] || exit 1
}
