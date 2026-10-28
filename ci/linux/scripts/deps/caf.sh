#!/usr/bin/env bash
# C++ Actor Framework.
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=../lib.sh
source "$HERE/../lib.sh"

require_env CAF_VERSION
log "Building C++ Actor Framework ${CAF_VERSION}"
git clone --depth 1 -b "$CAF_VERSION" https://github.com/actor-framework/actor-framework "$WORK/caf"
# CAF's ./configure is just a CMake wrapper; drive CMake directly so it uses
# the Ninja generator and installs to /usr/local/lib like the other deps
# (BUILD_SHARED_LIBS and CAF_ENABLE_IO_MODULE default ON; examples/tests off).
cmake_build "$WORK/caf" -DCAF_ENABLE_EXAMPLES=OFF -DCAF_ENABLE_TESTING=OFF
ldconfig
clean_work
