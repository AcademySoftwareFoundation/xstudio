#!/usr/bin/env bash
# Bootstrap vcpkg (if needed) and install the xSTUDIO manifest ports for Linux.
# Intended to run inside the toolchain container (or on a matching host devenv).
#
# Usage: ci/linux/scripts/build-deps.sh
#
# Required env:
#   VCPKG_ROOT
#   VCPKG_TARGET_TRIPLET   (x64-xstudio-linux-ubuntu2404 | x64-xstudio-linux-rocky9)
#
# Optional:
#   XSTUDIO_SRC=/src/xstudio
#   JOBS=1
#   VCPKG_BINARY_SOURCES
#   SKIP_VCPKG_BOOTSTRAP=1
#   FEED_URL / NUGET_OWNER / NUGET_PASSWORD|GITHUB_TOKEN — configure NuGet when set

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=build-common.sh
source "${SCRIPT_DIR}/build-common.sh"

: "${VCPKG_ROOT:?VCPKG_ROOT is required}"
: "${VCPKG_TARGET_TRIPLET:?VCPKG_TARGET_TRIPLET is required}"

SRC="${XSTUDIO_SRC:-/src/xstudio}"
if [ ! -f "${SRC}/vcpkg.json" ]; then
  echo "ERROR: vcpkg.json not found at ${SRC}" >&2
  exit 1
fi

# Host-mounted repos are owned by the runner UID; container often runs as root.
git config --global --add safe.directory "${VCPKG_ROOT}"
git config --global --add safe.directory "${SRC}"

VCPKG_BASELINE="$(
  python3 -c "import json,sys; print(json.load(open(sys.argv[1]))['builtin-baseline'])" \
    "${SRC}/vcpkg.json"
)"
: "${VCPKG_BASELINE:?could not read builtin-baseline from vcpkg.json}"

log "vcpkg root: ${VCPKG_ROOT} (baseline ${VCPKG_BASELINE})"
log "triplet:    ${VCPKG_TARGET_TRIPLET}"
log "jobs:       ${JOBS}"

if [ "${SKIP_VCPKG_BOOTSTRAP:-0}" != "1" ]; then
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
fi

if [ ! -x "${VCPKG_ROOT}/vcpkg" ]; then
  echo "ERROR: vcpkg binary missing at ${VCPKG_ROOT}/vcpkg" >&2
  exit 1
fi

if [ -n "${NUGET_PASSWORD:-${GITHUB_TOKEN:-}}" ] && [ -n "${FEED_URL:-}" ] && [ -n "${NUGET_OWNER:-}" ]; then
  NUGET_PASSWORD="${NUGET_PASSWORD:-${GITHUB_TOKEN}}"
  NUGET_SOURCE_NAME="${NUGET_SOURCE_NAME:-GitHubPackages}"
  NUGET_CONFIG_DIR="${NUGET_CONFIG_DIR:-${HOME}/.nuget/NuGet}"
  mkdir -p "${NUGET_CONFIG_DIR}"
  xml_escape() {
    printf '%s' "$1" | sed -e 's/&/\&amp;/g' -e 's/</\&lt;/g' -e 's/>/\&gt;/g' -e 's/"/\&quot;/g'
  }
  cat > "${NUGET_CONFIG_DIR}/NuGet.Config" <<EOF
<?xml version="1.0" encoding="utf-8"?>
<configuration>
  <packageSources>
    <add key="$(xml_escape "${NUGET_SOURCE_NAME}")" value="$(xml_escape "${FEED_URL}")" />
  </packageSources>
  <packageSourceCredentials>
    <$(xml_escape "${NUGET_SOURCE_NAME}")>
      <add key="Username" value="$(xml_escape "${NUGET_OWNER}")" />
      <add key="ClearTextPassword" value="$(xml_escape "${NUGET_PASSWORD}")" />
    </$(xml_escape "${NUGET_SOURCE_NAME}")>
  </packageSourceCredentials>
</configuration>
EOF
  log "NuGet config → ${NUGET_CONFIG_DIR}/NuGet.Config (${FEED_URL})"

  if command -v mono >/dev/null 2>&1; then
    nuget="$("${VCPKG_ROOT}/vcpkg" fetch nuget | tail -n 1)"
    if [ -f "${nuget}" ]; then
      mono "${nuget}" sources add \
        -Source "${FEED_URL}" \
        -StorePasswordInClearText \
        -Name "${NUGET_SOURCE_NAME}" \
        -UserName "${NUGET_OWNER}" \
        -Password "${NUGET_PASSWORD}" \
        || mono "${nuget}" sources update \
          -Source "${FEED_URL}" \
          -StorePasswordInClearText \
          -Name "${NUGET_SOURCE_NAME}" \
          -UserName "${NUGET_OWNER}" \
          -Password "${NUGET_PASSWORD}"
      mono "${nuget}" setapikey "${NUGET_PASSWORD}" -Source "${FEED_URL}"
      log "NuGet setapikey OK (${FEED_URL})"
    fi
  fi
fi

export VCPKG_DEFAULT_TRIPLET="${VCPKG_TARGET_TRIPLET}"
export VCPKG_MAX_CONCURRENCY="${JOBS}"

# Same install root as build-xstudio.sh (-DVCPKG_INSTALLED_DIR=.../vcpkg_installed).
# Explicit --x-install-root avoids drifting from cmake's default (build/vcpkg_installed).
log "Installing vcpkg manifest dependencies (${VCPKG_TARGET_TRIPLET})"
(
  cd "${SRC}"
  "${VCPKG_ROOT}/vcpkg" install \
    --triplet "${VCPKG_TARGET_TRIPLET}" \
    --overlay-ports="${SRC}/cmake/vcpkg_overlay_ports" \
    --overlay-triplets="${SRC}/cmake/vcpkg_triplets" \
    --x-install-root="${SRC}/vcpkg_installed"
)

log "Linux vcpkg deps OK"
