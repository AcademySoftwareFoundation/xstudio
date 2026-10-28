#!/usr/bin/env bash
# OpenColorIO.
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=../lib.sh
source "$HERE/../lib.sh"

require_env OCIO_VERSION
log "Building OpenColorIO ${OCIO_VERSION}"
fetch "https://github.com/AcademySoftwareFoundation/OpenColorIO/archive/refs/tags/v${OCIO_VERSION}.tar.gz" "ocio.tar.gz"
tar -xf "$WORK/ocio.tar.gz" -C "$WORK"
cmake_build "$WORK/OpenColorIO-${OCIO_VERSION}" \
  -DOCIO_BUILD_APPS=OFF -DOCIO_BUILD_TESTS=OFF -DOCIO_BUILD_GPU_TESTS=OFF \
  -DOCIO_BUILD_PYTHON=OFF -DOCIO_BUILD_DOCS=OFF
ldconfig
clean_work
