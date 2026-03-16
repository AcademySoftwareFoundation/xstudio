#!/usr/bin/env bash
# xSTUDIO Build Script for macOS / Linux
# Usage: ./build.sh [command] [options]
#
# Commands:
#   configure   - Run CMake configure step only
#   build       - Build xstudio (runs configure if needed)
#   deploy      - Copy built binaries to portable/
#   all         - configure + build + deploy (default)
#   clean       - Remove build directory
#   clean-qml   - Delete QML autogen cache (required after QML changes)
#
# Options:
#   --preset <name>   - CMake preset (default: auto-detected)
#   --config <type>   - Build type: Release|RelWithDebInfo|Debug (default: Release)
#   --target <name>   - Build target (default: xstudio)
#   --jobs <n>        - Parallel build jobs (default: $(nproc))
#   --no-deploy       - Skip deploy step in 'all' command

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# ---------- Defaults ----------
BUILD_DIR="build"
CONFIG="Release"
TARGET="xstudio"
JOBS=""
COMMAND="all"
PRESET=""
NO_DEPLOY=false

# ---------- Colour output ----------
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

info()  { echo -e "${CYAN}[INFO]${NC}  $*"; }
ok()    { echo -e "${GREEN}[OK]${NC}    $*"; }
warn()  { echo -e "${YELLOW}[WARN]${NC}  $*"; }
err()   { echo -e "${RED}[ERROR]${NC} $*" >&2; }

# ---------- Detect platform & preset ----------
detect_platform() {
    local os
    os="$(uname -s)"
    case "$os" in
        Darwin)
            local arch
            arch="$(uname -m)"
            if [[ "$arch" == "arm64" ]]; then
                echo "MacOSRelease"
            else
                echo "MacOSIntelRelease"
            fi
            ;;
        Linux)
            echo "LinuxRelease"
            ;;
        *)
            err "Unsupported platform: $os"
            exit 1
            ;;
    esac
}

resolve_preset() {
    if [[ -n "$PRESET" ]]; then
        echo "$PRESET"
        return
    fi

    local base
    base="$(detect_platform)"

    # Swap suffix based on CONFIG
    case "$CONFIG" in
        Release)         echo "$base" ;;
        RelWithDebInfo)  echo "${base%Release}RelWithDebInfo" ;;
        Debug)           echo "${base%Release}Debug" ;;
        *)               echo "$base" ;;
    esac
}

# ---------- Shared library extension ----------
lib_ext() {
    case "$(uname -s)" in
        Darwin) echo "dylib" ;;
        *)      echo "so" ;;
    esac
}

# ---------- Parse args ----------
parse_args() {
    while [[ $# -gt 0 ]]; do
        case "$1" in
            configure|build|deploy|all|clean|clean-qml)
                COMMAND="$1" ;;
            --preset)   shift; PRESET="$1" ;;
            --config)   shift; CONFIG="$1" ;;
            --target)   shift; TARGET="$1" ;;
            --jobs)     shift; JOBS="$1" ;;
            --no-deploy) NO_DEPLOY=true ;;
            -h|--help)
                sed -n '2,/^$/s/^# \?//p' "$0"
                exit 0 ;;
            *)
                err "Unknown argument: $1"
                exit 1 ;;
        esac
        shift
    done

    # Default job count
    if [[ -z "$JOBS" ]]; then
        if command -v nproc &>/dev/null; then
            JOBS="$(nproc)"
        elif command -v sysctl &>/dev/null; then
            JOBS="$(sysctl -n hw.ncpu)"
        else
            JOBS=4
        fi
    fi
}

# ---------- Commands ----------
do_configure() {
    local preset
    preset="$(resolve_preset)"
    info "Configuring with preset: ${preset}"
    cmake --preset "$preset"
    ok "Configure complete"
}

do_build() {
    # Configure if build dir doesn't exist
    if [[ ! -d "$BUILD_DIR" ]]; then
        warn "Build directory not found, running configure first..."
        do_configure
    fi

    info "Building target '${TARGET}' (${CONFIG}, ${JOBS} jobs)..."
    cmake --build "$BUILD_DIR" --config "$CONFIG" --target "$TARGET" -j "$JOBS"
    ok "Build complete"
}

do_deploy() {
    local ext
    ext="$(lib_ext)"

    info "Deploying to portable/..."

    # Ensure destination dirs exist
    mkdir -p portable/bin
    mkdir -p portable/share/xstudio/plugin

    # --- Main executable ---
    local exe_src="$BUILD_DIR/bin/xstudio"
    if [[ -f "$exe_src" ]]; then
        cp -v "$exe_src" portable/bin/
    elif [[ -f "$BUILD_DIR/bin/$CONFIG/xstudio" ]]; then
        cp -v "$BUILD_DIR/bin/$CONFIG/xstudio" portable/bin/
    else
        warn "xstudio executable not found in build output"
    fi

    # --- Core libraries (non-plugin) → portable/bin/ ---
    local core_libs=(
        "src/colour_pipeline/src"
        "src/module/src"
    )
    for lib_path in "${core_libs[@]}"; do
        local full="$BUILD_DIR/$lib_path"
        if [[ -d "$full" ]]; then
            find "$full" -maxdepth 1 -name "*.${ext}" -exec cp -v {} portable/bin/ \;
        fi
    done

    # --- Plugin libraries → portable/share/xstudio/plugin/ ---
    info "Deploying plugins..."
    local plugin_count=0
    while IFS= read -r -d '' plugin_lib; do
        cp -v "$plugin_lib" portable/share/xstudio/plugin/
        ((plugin_count++))
    done < <(find "$BUILD_DIR/src/plugin" -name "*.${ext}" -not -path "*/test/*" -print0 2>/dev/null || true)

    ok "Deployed ${plugin_count} plugins"
    ok "Deploy complete"
}

do_clean() {
    info "Removing build directory..."
    rm -rf "$BUILD_DIR"
    ok "Clean complete"
}

do_clean_qml() {
    info "Cleaning QML autogen cache..."
    rm -rf "$BUILD_DIR/src/launch/xstudio/src/xstudio_autogen/"
    ok "QML cache cleaned. Rebuild required."
}

# ---------- Main ----------
parse_args "$@"

case "$COMMAND" in
    configure)
        do_configure ;;
    build)
        do_build ;;
    deploy)
        do_deploy ;;
    all)
        do_configure
        do_build
        if [[ "$NO_DEPLOY" == false ]]; then
            do_deploy
        fi
        ;;
    clean)
        do_clean ;;
    clean-qml)
        do_clean_qml ;;
esac
