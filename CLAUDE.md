# xSTUDIO - Project Guide

## What is xSTUDIO?
Professional media playback and review application for VFX/film post-production.
- C++17/20, Qt6 QML frontend, CAF (C++ Actor Framework) for concurrency
- OpenEXR, FFmpeg media readers as plugins
- OpenGL GPU-accelerated viewport rendering
- Plugin architecture for media readers, colour operations, etc.

## Build System

**IMPORTANT: Always build as STANDALONE.** Building non-standalone causes linking/dependency issues on Windows.

### Windows Build (Primary)
```bash
# Configure (standalone)
cmake --preset WinRelease

# Build
cmake --build build --config Release --target xstudio

# The build output goes to: build/bin/Release/
```

### Portable Deployment (CRITICAL)
**The user runs xSTUDIO from `portable/bin/`, NOT from `build/bin/Release/`.**
After every build, you MUST copy updated binaries into the portable directory:
```bash
# Copy the main exe
cp build/bin/Release/xstudio.exe portable/bin/

# Copy any updated DLLs (plugin .dll files from build/bin/Release/)
cp build/bin/Release/*.dll portable/bin/
```
Without this step, the user will be running the OLD binary and won't see any changes.

### Key Build Details
- Generator: Visual Studio 17 2022
- vcpkg for package management (toolchain at ../vcpkg/)
- Qt6 at C:/Qt/6.5.3/msvc2019_64/
- Presets: WinRelease, WinRelWithDebInfo, WinDebug

## Architecture
- **Actor Model**: CAF-based distributed actors with message passing
- **Plugin System**: Dynamic loading for media readers, colour ops
- **Registries**: keyboard_events, media_reader_registry, plugin_manager_registry, etc.
- **Thread Pools**: OpenEXR internal (16 threads), 5 ReaderHelper actors, 4 detail readers

## Key Directories
```
src/plugin/media_reader/openexr/  - EXR reader plugin
src/media_reader/                  - Media reader coordination
src/media_cache/                   - Frame caching
src/ui/base/src/keyboard.cpp       - Hotkey system
src/ui/viewport/src/keypress_monitor.cpp - Key event distribution
src/ui/qml/viewport/src/hotkey_ui.cpp   - Hotkey QML model
src/playhead/                      - Playback control
```

## Test EXR Sequences
- `L:\tdm\shots\fw\9119\comp\images\FW9119_comp_v001\exr` (81 frames, 1000-1080)

## Working Style

**CRITICAL: Claude acts as ORCHESTRATOR, not implementor.**
- Spin up specialized agents (via Agent tool) for all development work: coding, debugging, building, benchmarking
- This prevents context window compaction and keeps the main conversation lean
- The orchestrator reads results from agents, coordinates next steps, and communicates with the user
- Only do trivial edits (like updating CLAUDE.md) directly — everything else gets delegated

## Issue Tracking
Uses **bd** (beads) for issue tracking. See AGENTS.md for workflow.
