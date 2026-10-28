#!/usr/bin/env bash
# LibRaw (RAW image support for OpenImageIO). Built after libjpeg-turbo.
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=../lib.sh
source "$HERE/../lib.sh"

require_env LIBRAW_VERSION
log "Building LibRaw ${LIBRAW_VERSION}"
git clone --depth 1 -b "$LIBRAW_VERSION" https://github.com/LibRaw/LibRaw.git "$WORK/libraw"
# LibRaw ships no CMakeLists of its own; overlay the build files from the
# companion LibRaw-cmake repo (method 1 of its README) and build with the
# standard helper. Examples/OpenMP/LCMS2/Jasper are off to avoid extra deps;
# OpenImageIO then auto-detects LibRaw (>= 0.18) and enables its RAW reader.
git clone --depth 1 https://github.com/LibRaw/LibRaw-cmake.git "$WORK/libraw-cmake"
cp -a "$WORK"/libraw-cmake/* "$WORK/libraw/"
cmake_build "$WORK/libraw" \
  -DENABLE_EXAMPLES=OFF -DENABLE_OPENMP=OFF -DENABLE_LCMS=OFF -DENABLE_JASPER=OFF
ldconfig
clean_work
