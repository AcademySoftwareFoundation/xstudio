#!/usr/bin/env bash
# OpenTimelineIO. Built after OpenEXR/Imath (OTIO_FIND_IMATH=ON).
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=../lib.sh
source "$HERE/../lib.sh"

require_env OTIO_REF
log "Building OpenTimelineIO (${OTIO_REF})"
git clone --depth 1 -b "$OTIO_REF" https://github.com/AcademySoftwareFoundation/OpenTimelineIO.git "$WORK/otio"
cmake_build "$WORK/otio" \
  -DOTIO_PYTHON_INSTALL=ON -DOTIO_DEPENDENCIES_INSTALL=OFF \
  -DOTIO_FIND_IMATH=ON -DOTIO_BUILD_TESTS=OFF
ldconfig
clean_work
