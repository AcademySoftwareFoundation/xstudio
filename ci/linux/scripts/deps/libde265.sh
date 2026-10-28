#!/usr/bin/env bash
# libde265 (HEVC decoder used by libheif). Built before libheif.
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=../lib.sh
source "$HERE/../lib.sh"

require_env LIBDE265_VERSION
log "Building libde265 ${LIBDE265_VERSION}"
# Library only: no dec265/sherlock265 example tools (which would also pull SDL).
git clone --depth 1 -b "v${LIBDE265_VERSION}" https://github.com/strukturag/libde265.git "$WORK/libde265"
cmake_build "$WORK/libde265" -DENABLE_DECODER=OFF -DENABLE_SDL=OFF
ldconfig
clean_work
