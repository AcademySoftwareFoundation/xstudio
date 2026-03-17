# xSTUDIO (fork)

xSTUDIO is a media playback and review application designed for professionals working in the film and TV post production industries, particularly the Visual Effects and Feature Animation sectors. xSTUDIO is focused on providing an intuitive, easy to use interface with a high performance playback engine at its core and C++ and Python APIs for pipeline integration and customisation for total flexibility.

This is a fork of the [original DNEG xSTUDIO](https://github.com/AcademySoftwareFoundation/xstudio) with significant enhancements for Windows usability, EXR performance, and day-to-day review workflows.

### Known Issues (upstream)

* Moderate audio distortion on playback (Windows only)
* User documentation and API documentation is out-of-date

---

## What's New in This Fork

### Filesystem Browser Plugin
New Python plugin with full Windows support for browsing and loading media directly from disk. Includes hierarchical directory tree, favorites/bookmarks, multi-select, and drag-drop into the timeline.

### Timeline Improvements
Zoom controls with zoom-to-fit, clip drag handles, and drag-drop from the browser. "Add to Timeline" context menu for quick media placement.

### EXR Layer/AOV Selector
Toolbar dropdown for selecting EXR layers and AOVs directly in the viewport, including support for multi-level hierarchical channel names (e.g. `crypto.R`, `deep.A`).

### Viewport Controls
Gamma and Saturation sliders in the viewport toolbar. Review mode flag for presentation workflows. Drag-drop media onto the viewport.

### Hotkey Editor
Full hotkey customization dialog — rebind any keyboard shortcut from the UI.

### EXR Performance
4 targeted fixes to the EXR read pipeline: optimized frame request queuing, reader actor improvements, and reduced overhead in the media cache path.

### Session Restore Fix
Fixed saved sessions not restoring correctly on Windows. Three path handling bugs in URI conversion (wrong variable in path remapping, case-sensitive drive letters, localhost authority in file URIs) plus a timing issue where the timeline panel wouldn't populate because the model tree hadn't finished loading.

### Windows Path Fixes
Resolved path handling bugs that broke EXR sequence loading on Windows (backslash/forward-slash normalization, drive letter handling).

### Cross-Platform Build Scripts
`build.bat` (Windows) and `build.sh` (macOS/Linux) for one-command builds with automatic preset detection.

---

## Building xSTUDIO

This release of xSTUDIO can be built on various Linux flavours, Microsoft Windows and MacOS. We provide comprehensive build steps here.

### Building xSTUDIO for Linux

* [Linux Generic](docs/reference/build_guides/linux_generic.md)
* [CentOS 7](docs/reference/build_guides/centos_7.md)
* [Rocky Linux 9.1](docs/reference/build_guides/rocky_linux_9_1.md)
* [Ubuntu 22.04](docs/reference/build_guides/ubuntu_22_04.md)

### Building xSTUDIO for Windows 10 & 11

* [Windows](docs/reference/build_guides/windows.md)

### Building xSTUDIO for MacOS

* [MacOS](docs/reference/build_guides/macos.md)

### Documentation Note

Note that the xSTUDIO user guide is built with Sphinx using the Read-The-Docs theme. The package dependencies for building the docs can be challenging to install so we instead include the fully built docs as part of xSTUDIO's repo, as well as the source for building the docs. Our build set-up by default disables the building of the docs to make life easy!
