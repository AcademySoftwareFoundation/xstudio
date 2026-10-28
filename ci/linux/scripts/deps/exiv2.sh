#!/usr/bin/env bash
# Exiv2 (EXIF/IPTC/XMP). Needed by the media_metadata_exif plugin on develop;
# distro packages are 0.27 and only export exiv2lib, while xSTUDIO links
# Exiv2::exiv2lib (0.28+ cmake namespace).
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=../lib.sh
source "$HERE/../lib.sh"

require_env EXIV2_VERSION
log "Building Exiv2 ${EXIV2_VERSION}"
git clone --depth 1 -b "v${EXIV2_VERSION}" https://github.com/Exiv2/exiv2.git "$WORK/exiv2"
# Library only: no CLI/samples/tests. Drop brotli/inih so we don't pull extra
# system packages; XMP uses system expat (expat-devel / libexpat-dev).
cmake_build "$WORK/exiv2" \
  -DEXIV2_BUILD_SAMPLES=OFF \
  -DEXIV2_BUILD_EXIV2_COMMAND=OFF \
  -DEXIV2_BUILD_UNIT_TESTS=OFF \
  -DEXIV2_ENABLE_BROTLI=OFF \
  -DEXIV2_ENABLE_INIH=OFF \
  -DEXIV2_ENABLE_VIDEO=OFF \
  -DEXIV2_ENABLE_WEBREADY=OFF
ldconfig
clean_work
