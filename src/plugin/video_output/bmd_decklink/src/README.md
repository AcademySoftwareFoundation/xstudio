# xstudio-decklink-plugin
A plugin for the xSTUDIO playback &amp; review application that allows SDI video output via Blackmagic Design Decklink PCI SDI Video Card.

## Dependencies
This plugin builds with no additional dependencies other than xSTUDIO itself. It currently only provides Linux compatibility and has been tested by us on Ubuntu 20.04LTS and Centos 7.6.
However, for it to work at runtime you will of course need the Blackmagic Design Decklink driver installed. Linux drivers can be downloaded from the [Blackmagic Design](https://www.blackmagicdesign.com/support/) website. Before attempting to use xSTUDIO with this plugin to output SDI we recommend that you test and validate your Decklink installation and video hardware using the Blackmagic Design configuration / diagnostic tools first.

## Building the Plugin

### Build and install xSTUDIO first!

*This repo currently only builds against a specific branch of the main xSTUDIO codebase which is not yet merged into the main repo. You must download xSTUDIO source from [this link](https://github.com/tedwaine/xstudio/tree/win_build_linux_compatibility) and build xSTUDIO first.*

Follow the build instructions proivided on the xSTUDIO repo. When invoking the cmake command to build xstudio, you should specify an install location for xSTUDIO. For example, let's say you've downloaded the xSTUDIO code from the link above (and you have already built and installed the dependencies according to the build instructions). If you are in the root folder of the repo you might execute these steps to specify where xstudio gets installed to.

`mkdir __build`

`cd __build`

`cmake .. -DCMAKE_INSTALL_PREFIX=/home/joebloggs/install`

`make -j`

`make install`

You can then run xstudio from its install locaation via the wrapper script. Following our example the command to run xSTUDIO would be

`/home/joebloggs/install/xstudio/bin/xstudio`

### Building the Decklink plugin

Clone this repo first. Then in the root directory make a build folder. You may need to tell cmake where xstudio is so it can find the xSTUDIO headers and libraries needed to build the plugin. This can be done by setting an 'xStudio_DIR' environment variable pointing to xSTUIDIO's installed cmake modules. This step depends on your build environment. See the example below. Note that for this to work correctly xSTUDIO must be installed to the same location allowing for the plugin to be placed in xSTUDIO's local plugins directory. Alternatively you can use the environment variable XSTUDIO_PLUGIN_PATH to specify a colon separated list of paths for xSTUDIO to scan for plugin installations.

`mkdir __build`

`cd __build`

`export xStudio_DIR=/home/ted/install/xstudio/lib/cmake/xStudio`

`cmake .. -DCMAKE_INSTALL_PREFIX=/home/joebloggs/install`

`make -j`

`make install`

## Using the plugin

Once you have successfully built and installed the plugin, start xstudio from its install location via the wrapper as shown above. You should see a button appear in the top left of the xSTUDIO viewport with a movie camera icon. Click this button to access the Decklink control panel. Here you set the video output mode including resolution, frame rate and pixel format. Enable the output via the big On/Off switch. There is an audio delay setting that allows you to tune for any audio latency in your A/V set-up if you find that sound is not synced accurately with video (though in most cases this shouldn't be an issue).
