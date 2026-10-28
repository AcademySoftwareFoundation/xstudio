#!/usr/bin/env bash
# libheif (HEIF/HEIC support for OpenImageIO). Built after libde265 and codecs.
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=../lib.sh
source "$HERE/../lib.sh"

require_env LIBHEIF_VERSION
log "Building libheif ${LIBHEIF_VERSION}"
# Picks up libde265 (decoder) and x265 (encoder) from /usr/local (Rocky) or
# the system (Ubuntu) via pkg-config. AVIF/AOM is off to avoid a libaom dep;
# OpenImageIO then auto-detects libheif (>= 1.3) and enables its HEIF reader.
git clone --depth 1 -b "v${LIBHEIF_VERSION}" https://github.com/strukturag/libheif.git "$WORK/libheif"
cmake_build "$WORK/libheif" \
  -DWITH_EXAMPLES=OFF -DWITH_GDK_PIXBUF=OFF -DBUILD_TESTING=OFF \
  -DWITH_AOM_DECODER=OFF -DWITH_AOM_ENCODER=OFF
ldconfig
clean_work
