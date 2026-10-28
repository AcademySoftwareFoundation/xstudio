#!/usr/bin/env bash
# OpenImageIO. Built last so it auto-detects OpenEXR, OCIO, FFmpeg, FreeType,
# libjpeg-turbo, LibRaw and libheif installed by the steps above. Library only:
# no Python bindings, command-line tools, viewer or tests.
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=../lib.sh
source "$HERE/../lib.sh"

require_env OIIO_VERSION
log "Building OpenImageIO ${OIIO_VERSION}"
git clone --depth 1 -b "v${OIIO_VERSION}" https://github.com/AcademySoftwareFoundation/OpenImageIO.git "$WORK/oiio"
cmake_build "$WORK/oiio" \
  -DUSE_PYTHON=OFF \
  -DOIIO_BUILD_TOOLS=OFF \
  -DOIIO_BUILD_TESTS=OFF \
  -DUSE_QT=OFF \
  -DSTOP_ON_WARNING=OFF
ldconfig
clean_work
