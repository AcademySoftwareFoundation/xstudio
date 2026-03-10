.. _api_overview:

C++ API Overview
================

xSTUDIO's C++ API is plugin-first. The public headers in `include/xstudio`
define the core extension points, while concrete examples live in `src/plugin`.

What This Reference Covers
--------------------------

The pages in this section expose the public headers that are most useful when
extending xSTUDIO:

* `plugin_manager/plugin_base.hpp` for `StandardPlugin` and viewport-related
  hooks.
* `ui/viewport/video_output_plugin.hpp` for off-screen render and video-output
  integrations.
* `colour_pipeline/colour_pipeline.hpp` for display and colour-management
  plugins.
* `ui/viewport/viewport_layout_plugin.hpp` and
  `plugin_manager/hud_plugin.hpp` for compare layouts and HUD or overlay
  plugins.
* `data_source/data_source.hpp`, `media_reader/media_reader.hpp`,
  `media_metadata/media_metadata.hpp`, and `media_hook/media_hook.hpp` for
  ingest and metadata customization.
* `utility/*.hpp` for the shared value types, JSON helpers, frame and timecode
  types, UUID helpers, and other support classes used throughout the API.

Reference Implementations
-------------------------

The fastest way to understand the API is usually to read the shipped plugins
alongside the header docs:

* `src/plugin/colour_pipeline/ocio` shows the built-in OCIO integration.
* `src/plugin/viewport_layout/*` contains the default, grid, composite, and
  wipe layout implementations.
* `src/plugin/hud/*` contains concrete HUD overlays such as pixel probe and EXR
  data window.
* `src/plugin/media_reader/*` and `src/plugin/media_metadata/*` contain file
  format readers and metadata readers.
* `src/plugin/data_source/*` contains data-source integrations, including the
  DNEG-specific examples.

Reading Strategy
----------------

If you are new to the codebase, start with `StandardPlugin`, then move to the
specific plugin type you need. After that, inspect the matching implementation
under `src/plugin` and the relevant utility headers used by that plugin.
