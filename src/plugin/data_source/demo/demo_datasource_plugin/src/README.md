# XStudio Demo DataSource Plugin

This is the documentation for xStudio demo datasource plugin. This plugin has been added to the project to help developers that are wishing to write their own plugins for xSTUDIO. In particular if they are interested in writing a plugin to interface with a production database, say, and add a UI panel to xSTUDIO so that users can interact with the database to load media, playlists and timelines.

The plugin demonstrates a number of key API features for plugin development plus some general Qt/QML examples which could be useful starting points for developing fast and sophisticated database front-ends. The code is heavily annotated with inline comments to explain what's going on.

- Instance your own custom 'Panel' that can embed in xSTUDIO or exist as a floating window.
- Inject menu items into xSTUDIO's menu system (from C++, Python and QML)
- Implement a custom tree view in QML, with a simple C++ backend, as a way of exposing heirarchical database information.
- Interfacing from QML via C++ with a (very simple mock) database whose API is Python based, to build UI elements.
- Implement a simple 'flat' list of dynamic data that drives a UI list view widget.
- Implementing a custom media reader that generates images procedurally both on the CPU and GPU.
- Adding an extra 'MediaSource' to a newly created Media item (in this case we add a lower-res proxy image file)
- How to send messages and data between Python code and a C++ plugin
- Generate an OTIO timeline using OTIO python api and load the timeline into xSTUDIO

## Building the Plugin

If you are using the CMakePresets.json file (i.e. as the first step to build xSTUDIO you are running cmake and using the --preset argument) then add this key value pair to the "cacheVariables" dictionary.

"STUDIO_PLUGINS": "demo"

Alternatively edit the plugin/src/data_source/CMakeLists.txt file as per indication in that file.

## Running the Plugin

Once built, you should see a plug icon appear in the shelf of buttons in the action bar to the top of the viewport. This will show the demo interface in a floating window. Alternatively the 'Demo PLugin' option should appear on the tab dropdown options for the xSTUDIO panel tabs, allowing you to fill a panel with the demo plugin interface.