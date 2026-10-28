#!/usr/bin/env bash
# libjpeg-turbo (installed under /usr/local; OIIO needs >= 2.1).
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=../lib.sh
source "$HERE/../lib.sh"

require_env LIBJPEGTURBO_VERSION
log "Building libjpeg-turbo ${LIBJPEGTURBO_VERSION}"
fetch "https://github.com/libjpeg-turbo/libjpeg-turbo/releases/download/${LIBJPEGTURBO_VERSION}/libjpeg-turbo-${LIBJPEGTURBO_VERSION}.tar.gz"
tar -xf "$WORK/libjpeg-turbo-${LIBJPEGTURBO_VERSION}.tar.gz" -C "$WORK"
cmake_build "$WORK/libjpeg-turbo-${LIBJPEGTURBO_VERSION}" -DENABLE_STATIC=OFF
ldconfig
clean_work
