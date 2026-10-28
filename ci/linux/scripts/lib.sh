#!/usr/bin/env bash
# Shared helpers, sourced by every build script.
# Containers run as root, so no sudo is needed.
#
# Dependency pins come from ENV (Dockerfile ARG <- just <- versions.env).
set -euo pipefail

# Parallelism: honour an explicit JOBS, otherwise 8 threads.
export JOBS="${JOBS:-8}"

# Where sources are downloaded and built. Wiped between deps to keep layers small.
export WORK="${WORK:-/tmp/xbuild}"
mkdir -p "$WORK"

# Source-built dependencies install here; both the build and the runtime
# bundling step look for them in these locations.
export PKG_CONFIG_PATH="/usr/local/lib64/pkgconfig:/usr/local/lib/pkgconfig:${PKG_CONFIG_PATH:-}"
export LD_LIBRARY_PATH="/usr/local/lib:/usr/local/lib64:${LD_LIBRARY_PATH:-}"

# Prefer the Ninja generator for CMake builds when it is available: it is
# honoured by every `cmake -S ... -B ...` invocation without per-call flags
# (and `cmake --build` stays generator-agnostic). Autotools / plain-Makefile
# deps (glew, nasm, x264, fdk-aac, ffmpeg) are unaffected and keep make.
if command -v ninja >/dev/null 2>&1; then
    export CMAKE_GENERATOR="${CMAKE_GENERATOR:-Ninja}"
fi

log() { printf '\n\033[1;34m==> %s\033[0m\n' "$*"; }

# fetch <url> [outfile] -> downloads into $WORK
fetch() {
    # Two statements: in a single `local`, all right-hand sides are expanded
    # before any assignment takes effect, so `$url` would be unbound (set -u)
    # when the `out` default runs basename on it.
    local url="$1"
    local out="${2:-$(basename "$url")}"
    log "Downloading $url"
    wget --quiet --tries=3 --timeout=60 -O "$WORK/$out" "$url"
}

# require_env VAR1 VAR2 ... -> fail early if a pinned version is missing
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

# cmake_build <srcdir> [extra cmake args...] -> configure, build and install a
# CMake project into /usr/local. Shared by every CMake-based dependency script.
cmake_build() {
    local src="$1"
    shift
    # Pin the install prefix explicitly: most projects default to /usr/local,
    # but some (e.g. OpenImageIO) override that default to an in-tree ./dist
    # directory when no prefix is given, which would then be wiped with $WORK.
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

# clean_work -> drop the source/build tree once a dependency is installed, so
# each per-dependency Docker layer stays small. $WORK is recreated above when
# lib.sh is next sourced (i.e. on the next RUN).
clean_work() { rm -rf "$WORK"; }
