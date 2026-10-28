#!/usr/bin/env bash
# fmt (installed under /usr/local).
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=../lib.sh
source "$HERE/../lib.sh"

require_env FMT_VERSION
log "Building fmt ${FMT_VERSION}"
fetch "https://github.com/fmtlib/fmt/archive/refs/tags/${FMT_VERSION}.tar.gz" "fmt.tar.gz"
tar -xf "$WORK/fmt.tar.gz" -C "$WORK"
cmake_build "$WORK/fmt-${FMT_VERSION}" -DFMT_DOC=OFF -DFMT_TEST=OFF
clean_work
