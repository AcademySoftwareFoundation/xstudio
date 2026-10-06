# Changelog

All notable changes to xSTUDIO are documented in this file. User-facing release notes are in `docs/user_docs/release_notes/index.rst`.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [Unreleased] - v1.4.x

Covers changes after commit `eb3d235e`.

### Added

- Compare multiple timelines at once with a shared playhead.
- 'Manual' frame align mode for compare modes, with per-source offsets that persist in the session.
- Hotkey to split a timeline clip at the current frame (default `Meta+S`).
- Numeric keypad keys can be used as independent hotkeys.
- Escape hotkey acts as a workflow reset: it resets the viewport and exits fullscreen.
- Media panel scroll features and additional hotkeys.
- Filesystem browser feature additions and improvements.
- macOS: "Open With" support in the Finder context menu.
- Apple modifier key names are displayed in the UI.
- Portable Windows packaging target and a Windows build automation script (`scripts/build_windows.ps1`).
- GTest enabled on Windows; docs for running the unit tests on Windows.
- Python API: viewport scale and pan atoms.
- Python API: annotation event group and a subscription mechanism on the plugin base class.
- Python API: `BookmarkDetail.subject` is now exposed.

### Changed

- **Breaking (Python API):** `Vec2f` renamed to `V2f` to align with conventions.
- Python connection returns a response that arrived instead of reporting a timeout.
- Embedded Python interpreter connects to the API actor directly rather than over TCP.
- "Create new note" hotkey does nothing if an empty note already exists on the current frame.
- Metadata overlays are not rendered when the output resolution is small (e.g. thumbnails).
- OTIO import maps `ImageSequenceReference` clip trims into media frame space.
- macOS: Python and QML files relocated to allow codesigning.
- macOS: `install_name` and rpath are set at link time; GLEW dependencies removed; XQuartz prefixes ignored in `find_package()`.
- Several message handlers updated to modern syntax to avoid warnings; no-op handlers added to avoid error messages.
- Documentation: Read the Docs build configuration fixed; Windows and macOS build guides updated.

### Fixed

- Python: typo in `subset.py` prevented `parent_playlist()` from working.
- Python: event group callback routing in the Python actor.
- Blender multilayer EXR decoding.
- PDF reader producing garbled black results.
- Grading mask not shown with a core-profile OpenGL context.
- OCIO: shader and thumbnail views were discarded when the display was empty (offscreen renders, client-look double grade).
- OTIO reader: replaced `clip_if()`, which was removed from OpenTimelineIO, with `find_clips`.
- Windows: UNC paths handled with both separator styles; UNC path only built from a file URI.
- Linux leading-slash path fixup now applied on all platforms.
- macOS: app-level snippets were missing.
- `Attribute::uuid()` defined out-of-line so its `any_cast` cannot mismatch.
- Error message when the number of columns is small (array length check).
