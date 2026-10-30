#!/usr/bin/env bash
# GLEW (installed under /usr/local).
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=../build-common.sh
source "$HERE/../build-common.sh"

require_env GLEW_VERSION
log "Building GLEW ${GLEW_VERSION}"
fetch "https://github.com/nigels-com/glew/releases/download/glew-${GLEW_VERSION}/glew-${GLEW_VERSION}.tgz"
tar -xf "$WORK/glew-${GLEW_VERSION}.tgz" -C "$WORK"
cmake_build "$WORK/glew-${GLEW_VERSION}/build/cmake" -DBUILD_UTILS=OFF
ldconfig
clean_work
