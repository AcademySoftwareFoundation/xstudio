#!/usr/bin/env bash
# C++ fileseq (justinfx/gofileseq). Only the cpp/ tree is built; installs to
# /usr/local so find_package(FileSeq) picks it up like the other source deps.
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=../lib.sh
source "$HERE/../lib.sh"

require_env FILESEQ_CPP_REF
log "Building fileseq C++ (${FILESEQ_CPP_REF})"
git clone --depth 1 -b "$FILESEQ_CPP_REF" https://github.com/justinfx/gofileseq.git "$WORK/gofileseq"
cmake_build "$WORK/gofileseq/cpp"
# Upstream install(FILES ...) ships the top-level headers only; frameset.h
# includes "ranges/ranges.h" (resolved next to the installed header), so copy
# that public header into the same tree. See justinfx/gofileseq cpp/CMakeLists.
mkdir -p /usr/local/include/fileseq/ranges
cp -a "$WORK/gofileseq/cpp/ranges/ranges.h" /usr/local/include/fileseq/ranges/
ldconfig
clean_work
