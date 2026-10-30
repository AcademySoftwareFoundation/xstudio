#!/usr/bin/env bash
# Shared helpers for Linux CI build scripts (tools, deps, xSTUDIO).
# Sourced by every script under ci/linux/scripts/.
#
# Provides: JOBS, WORK, PKG_CONFIG_PATH, LD_LIBRARY_PATH, CMAKE_GENERATOR,
#           log, fetch, require_env, source_versions_env, cmake_build, clean_work
#
# Callers must set version pins via Docker ENV or source_versions_env + require_env.

set -euo pipefail

export JOBS="${JOBS:-1}"

export WORK="${WORK:-/tmp/xbuild}"
mkdir -p "$WORK"

export PKG_CONFIG_PATH="/usr/local/lib64/pkgconfig:/usr/local/lib/pkgconfig:${PKG_CONFIG_PATH:-}"
export LD_LIBRARY_PATH="/usr/local/lib:/usr/local/lib64:${LD_LIBRARY_PATH:-}"

# Prefer Ninja for every `cmake -S … -B …` when available (autotools deps unaffected).
if command -v ninja >/dev/null 2>&1; then
    export CMAKE_GENERATOR="${CMAKE_GENERATOR:-Ninja}"
fi

log() { printf '\n\033[1;34m==> %s\033[0m\n' "$*"; }

# fetch <url> [outfile] — download into $WORK
fetch() {
    # Split locals: with one `local`, RHS expand before assignment, so `$url` is unbound under set -u.
    local url="$1"
    local out="${2:-$(basename "$url")}"
    log "Downloading $url"
    wget --quiet --tries=3 --timeout=60 -O "$WORK/$out" "$url"
}

# source_versions_env — load ci/linux/versions.env when present (local runs; no-op in Docker).
source_versions_env() {
    local versions
    versions="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)/versions.env"
    if [ -f "$versions" ]; then
        # shellcheck source=../versions.env
        source "$versions"
    fi
}

# require_env VAR1 VAR2 ... — fail early if a pinned version is missing
require_env() {
    local missing=0 v
    for v in "$@"; do
        if [ -z "${!v:-}" ]; then
            echo "ERROR: required pin '$v' is not set (check versions.env)" >&2
            missing=1
        fi
    done
    [ "$missing" -eq 0 ] || exit 1
}

# cmake_build <srcdir> [extra cmake args...] — configure, build, install to /usr/local
cmake_build() {
    local src="$1"
    shift
    # Always pass INSTALL_PREFIX: some projects (e.g. OpenImageIO) default to ./dist otherwise.
    cmake -S "$src" -B "$src/build" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_LIBDIR=lib \
        -DCMAKE_INSTALL_PREFIX=/usr/local \
        -DBUILD_SHARED_LIBS=ON \
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
        -DBUILD_TESTING=OFF \
        "$@"
    cmake --build "$src/build" -j "$JOBS"
    cmake --install "$src/build"
}

# clean_work — drop $WORK after a dep install so Docker layers stay small
clean_work() { rm -rf "$WORK"; }
