#!/usr/bin/env bash
# FFmpeg. Built after the codec libraries it enables.
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=../lib.sh
source "$HERE/../lib.sh"

require_env FFMPEG_VERSION
log "Building FFmpeg ${FFMPEG_VERSION}"
fetch "https://ffmpeg.org/releases/ffmpeg-${FFMPEG_VERSION}.tar.bz2"
tar -xf "$WORK/ffmpeg-${FFMPEG_VERSION}.tar.bz2" -C "$WORK"
(cd "$WORK/ffmpeg-${FFMPEG_VERSION}" &&
  ./configure \
    --prefix=/usr/local \
    --enable-gpl --enable-nonfree --enable-shared \
    --disable-static --enable-pic --disable-doc \
    --enable-libfdk_aac --enable-libfreetype --enable-libmp3lame \
    --enable-libopus --enable-libvpx --enable-libx264 --enable-libx265 &&
  make -j "$JOBS" && make install)
ldconfig
clean_work
