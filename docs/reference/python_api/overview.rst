.. _python_api_overview:

Python API Overview
===================

The Python package in this repository exposes three complementary layers:

* `xstudio.connection` for connecting to a running xSTUDIO process.
* `xstudio.api` for high-level wrappers around sessions, playlists, timelines,
  media, viewports, and other application actors.
* `xstudio.plugin` for authoring Python plugins and UI extensions.

Connection Model
----------------

The usual entry point is `xstudio.connection.Connection`. Once connected, use
the `api` property to access the higher-level wrapper classes:

.. code-block:: python

    from xstudio.connection import Connection

    connection = Connection(auto_connect=True)
    api = connection.api

    print(api.host, api.port)
    print(api.session.playlists)

This same structure works for remote control, embedded scripting, and plugin
code. The main difference is where the `Connection` comes from and whether
xSTUDIO is already running.

Main Object Graph
-----------------

At a high level, the object model looks like this:

* `Connection` owns the transport and message loop.
* `API` exposes application-level services such as the current session,
  caches, plugin manager, and scanner.
* `Studio` and `App` wrap the running application.
* `Session` exposes playlists, subsets, contact sheets, sequences, media,
  bookmarks, and playheads.
* `Viewport`, `Playhead`, and `ColourPipeline` provide viewer-specific control.

Where To Look In The Source Tree
--------------------------------

* `python/src/xstudio/api` contains the main high-level wrappers.
* `python/src/xstudio/plugin` contains Python plugin base classes.
* `python/src/xstudio/demo` contains small example scripts.
* `src/plugin/python_plugins` contains shipped plugin examples that integrate
  with the UI.
