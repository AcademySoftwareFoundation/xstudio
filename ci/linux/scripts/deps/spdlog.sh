#!/usr/bin/env bash
# spdlog (installed under /usr/local; needs fmt first).
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=../lib.sh
source "$HERE/../lib.sh"

require_env SPDLOG_VERSION
log "Building spdlog ${SPDLOG_VERSION}"
fetch "https://github.com/gabime/spdlog/archive/refs/tags/v${SPDLOG_VERSION}.tar.gz" "spdlog.tar.gz"
tar -xf "$WORK/spdlog.tar.gz" -C "$WORK"
cmake_build "$WORK/spdlog-${SPDLOG_VERSION}" -DSPDLOG_BUILD_SHARED=ON -DSPDLOG_FMT_EXTERNAL=ON
clean_work
