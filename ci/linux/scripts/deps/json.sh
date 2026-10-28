#!/usr/bin/env bash
# nlohmann-json (installed under /usr/local).
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=../lib.sh
source "$HERE/../lib.sh"

require_env JSON_VERSION
log "Building nlohmann-json ${JSON_VERSION}"
fetch "https://github.com/nlohmann/json/archive/refs/tags/v${JSON_VERSION}.tar.gz" "json.tar.gz"
tar -xf "$WORK/json.tar.gz" -C "$WORK"
cmake_build "$WORK/json-${JSON_VERSION}" -DJSON_BuildTests=OFF -DNLOHMANN_JSON_CONFIG_INSTALL_DIR=lib/cmake/nlohmann_json
clean_work
