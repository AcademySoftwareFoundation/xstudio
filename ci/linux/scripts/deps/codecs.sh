#!/usr/bin/env bash
# FFmpeg codec libraries (nasm, x264, x265, fdk-aac) under /usr/local.
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=../lib.sh
source "$HERE/../lib.sh"

require_env NASM_VERSION X264_REF X265_VERSION FDKAAC_REF

log "Building NASM ${NASM_VERSION}"
fetch "https://www.nasm.us/pub/nasm/releasebuilds/${NASM_VERSION}/nasm-${NASM_VERSION}.tar.bz2"
tar -xf "$WORK/nasm-${NASM_VERSION}.tar.bz2" -C "$WORK"
(cd "$WORK/nasm-${NASM_VERSION}" && ./configure --prefix=/usr/local && make -j "$JOBS" && make install)

log "Building x264 (${X264_REF})"
git clone --branch "$X264_REF" --depth 1 https://code.videolan.org/videolan/x264.git "$WORK/x264"
(cd "$WORK/x264" && ./configure --prefix=/usr/local --enable-shared && make -j "$JOBS" && make install)

log "Building x265 ${X265_VERSION}"
# Bitbucket downloads redirect/fail intermittently; VideoLAN mirrors the release tarballs.
fetch "https://download.videolan.org/pub/contrib/x265/x265_${X265_VERSION}.tar.gz"
tar -xf "$WORK/x265_${X265_VERSION}.tar.gz" -C "$WORK"
# x265 does not go through cmake_build (source lives under source/); mirror the
# shared defaults plus ENABLE_CLI=OFF — the encoder CLI is not bundled.
cmake -S "$WORK/x265_${X265_VERSION}/source" -B "$WORK/x265_${X265_VERSION}/build" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/usr/local \
  -DBUILD_SHARED_LIBS=ON \
  -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
  -DBUILD_TESTING=OFF \
  -DENABLE_SHARED=ON \
  -DENABLE_CLI=OFF
cmake --build "$WORK/x265_${X265_VERSION}/build" -j "$JOBS"
cmake --install "$WORK/x265_${X265_VERSION}/build"

log "Building fdk-aac (${FDKAAC_REF})"
git clone --depth 1 --branch "$FDKAAC_REF" https://github.com/mstorsjo/fdk-aac "$WORK/fdk-aac"
(cd "$WORK/fdk-aac" && autoreconf -fiv && ./configure --prefix=/usr/local --enable-shared && make -j "$JOBS" && make install)

ldconfig
clean_work
