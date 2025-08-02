# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview

xSTUDIO is a professional media playback and review application designed for the film and TV post-production industries, particularly VFX and feature animation. It features a high-performance playback engine built with C++ and Python APIs for pipeline integration and customization.

## Core Architecture

xSTUDIO is built on a modular, actor-based architecture using the C++ Actor Framework (CAF):

- **Actor System**: The core is built around CAF actors that communicate via message passing
- **Registry System**: Components are registered in named registries (e.g., `global_registry`, `plugin_manager_registry`, `media_reader_registry`)
- **Plugin Architecture**: Extensible plugin system supporting multiple plugin types (colour ops, media readers, data sources, viewport overlays, etc.)
- **Session Management**: Sessions contain playlists, timelines, and media sources with hierarchical organization
- **Media Pipeline**: Handles media reading, caching, metadata extraction, and playback
- **UI Framework**: Qt5/QML-based interface with OpenGL viewport rendering

### Key Components

- **Global Actor** (`src/global/`): Central coordinator and message router
- **Studio Actor** (`src/studio/`): Top-level application container
- **Session System** (`src/session/`): Project/session management
- **Media System** (`src/media/`): Media sources, streams, and metadata handling
- **Playhead** (`src/playhead/`): Playback control and timeline navigation
- **Plugin Manager** (`src/plugin_manager/`): Dynamic plugin loading and management
- **Viewport** (`src/ui/viewport/`): OpenGL-based media display and rendering
- **Python Integration** (`src/embedded_python/`, `src/python_module/`): Python API and scripting

## Development Commands

### Building
```bash
# Configure build with CMake
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build (adjust -j based on CPU cores)
make -j$(nproc)

# Build specific target
make xstudio
```

### Testing
```bash
# Build with tests enabled
cmake .. -DBUILD_TESTING=ON

# Run C++ tests
make test

# Run Python tests (requires built xstudio)
cd python/test
pytest
```

### Code Quality
```bash
# Format C++ code
ninja clangformat  # if ENABLE_CLANG_FORMAT=ON

# Static analysis
ninja clang-tidy   # if ENABLE_CLANG_TIDY=ON
```

### Running
```bash
# Start xstudio
./build/bin/xstudio.bin

# Start with specific session
./build/bin/xstudio.bin --session "session_name"

# Headless mode for testing
./build/bin/xstudio.bin -e -n --log-file xstudio.log

# Python control scripts
xstudio-control   # CLI control interface
xstudio-inject    # Inject commands into running instance
```

## File Organization

### Core Libraries (`src/`)
- `atoms.hpp`: CAF message atom definitions (central to actor communication)
- `*/src/`: Implementation files for each component
- `*/test/`: Unit tests for each component
- `plugin/`: Plugin implementations organized by type

### UI Components (`src/ui/`)
- `qml/`: QML UI components and models
- `opengl/`: OpenGL rendering and shaders
- `viewport/`: Main viewport implementation

### Python Integration (`python/`)
- `src/xstudio/`: Python API modules
- `test/`: Python test suite using pytest
- Uses pybind11 for C++/Python binding

### Configuration (`share/`)
- `preference/`: JSON preference files for core and plugins
- `fonts/`: Application fonts
- `snippets/`: Code/configuration snippets

## Plugin Development

Plugins inherit from base classes in `include/xstudio/plugin_manager/plugin_base.hpp`:

- **ColourOp**: Image processing operations
- **MediaReader**: Media file format support
- **DataSource**: External data integration (e.g., Shotgun)
- **ViewportOverlay**: HUD and overlay elements
- **MediaHook**: Pipeline integration hooks

Plugin registration uses factory pattern and CMake auto-discovery.

## Testing Strategy

- **C++ Tests**: Unit tests using framework in `test/` directories
- **Python Tests**: pytest-based tests in `python/test/`
- **Integration Tests**: End-to-end tests via Python API
- **Test Resources**: Sample media files in `test_resource/`

## Key Conventions

- **Message Passing**: Use CAF atoms for inter-actor communication
- **UUID Usage**: UUIDs identify sessions, media, playlists throughout system  
- **JSON Configuration**: Preferences and settings stored as JSON
- **Frame-based Operations**: Timeline operations use frame-based addressing
- **Plugin Lifecycle**: Plugins loaded dynamically at startup and managed centrally

## Common Patterns

- Actor spawning via `spawn_actor_in_group()` with registry registration
- Asynchronous operations using CAF's `request().then()` pattern
- UI updates via Qt signal/slot mechanism bridged to actor messages
- Media metadata cached and shared across components
- OpenGL rendering with shader program abstractions

## Build Dependencies

See `docs/build_guides/` for platform-specific dependency installation. Key dependencies include Qt5, OpenEXR, CAF, pybind11, OpenGL, and various media libraries for format support.