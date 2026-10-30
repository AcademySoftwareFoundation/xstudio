#!/usr/bin/env bash
# OpenEXR (also installs Imath). Built before OTIO/OIIO.
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=../build-common.sh
source "$HERE/../build-common.sh"

require_env OPENEXR_REF
log "Building OpenEXR ${OPENEXR_REF} (also installs Imath)"
git clone --depth 1 -b "$OPENEXR_REF" https://github.com/AcademySoftwareFoundation/openexr.git "$WORK/openexr"
cmake_build "$WORK/openexr" -DOPENEXR_BUILD_TOOLS=OFF
ldconfig
clean_work
