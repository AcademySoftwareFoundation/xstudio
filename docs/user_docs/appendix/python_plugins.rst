.. _python_plugins:

##############
Python Plugins
##############

Introduction
------------

xSTUDIO has a Python plugin API through the provision of plugin base classes which you can use to build your own xSTUDIO extensions. The plugin base classes allow many ways to interact with the application and the session data:

    * Interact with xSTUDIO's core session obejct
        * Build and manage Playlists
        * Concurrent media creation for rapid loading
        * Build or load Sequence objects for multi-track timelines
        * Import / Export OpenTimelineIO files for timeline interchange
    * Interact with xSTUDIO's playback engine, viewport and colour management components
        * Start/stop playback, position the playhead, jump through media
        * Dynamically set-up media comparison (A/B, Wipe, Grid (Contact Sheet)) modes
        * Dynamically control colour management data (Change exposure, apply grades)
        * Spawn 'QuickView' player windows to review individual media items outside of the main interface.
    * Add attribute data items to your plugin. 
        * Attributes are flexible compound data types that can represent a wide range of data types:
            * boolean
            * integer number
            * string 
            * real number
            * list
            * dictionary (Json formatable)
        * Attribute data can be accessed from QML code directly
        * Sets of attributes can be accessed in QML as dynamic 'model' data aligned with Qt's Model-View-Delegate design patterns
    * Create custom UI 'panels' that embed in the xSTUDIO UI through Qt QML
    * Create custom Qt QML floating windows or dialogs
    * Create custom overlay QML graphics for the Viewport for any data visualisation you require
    * Inject menu items in xSTUDIO's menu system
    * Add hotkey shortcuts that run your own callbacks when triggered
    * Receive callbacks from UI code (Qt QML)
    * Add custom items to the Viewport toolbar
    * Subscribe to event messages that are broadcast by various xSTUDIO components
        * Get notification when the on-screen Media item changes
        * Get notification when the playhead state (position, playing, compare mode etc) changes
        * Get notification when a new Viewport has been created

.. note::
    Because xSTUDIO's embedded Python interpreter runs in a separate thread to the main UI event loops the execution of Python code will not slow or freeze xSTUDIO's interface. It's even possible to run code in a Python plugin that builds large playlists and loads lots of new media without xSTUDIO's Viewport and playback engine being delayed so smooth and accurate playback can continue in the foreground.

Authoring Plugins
-----------------

xSTUDIO loads Python plugins on start-up. There is a preference setting (**Python Plugin Path** in the **Python** section of the :ref:`Preferences Manager <preferences>`) where you can set one or more filesystem directories for xSTUDIO to scan for your plugins. In addition to this xSTUDIO will also search the directories in the environment variable **XSTUDIO_PYTHON_PLUGIN_PATH** (a colon separated list on Mac/Linux or semi-colon sparated on Windows).

Each plugin should be installed as a Python *importable* module in its own folder. The interface of the plugin python module must include a method **create_plugin_instance** that returns and instance of your plugin class (see exampled below).

Bare Bones Plugin Example
^^^^^^^^^^^^^^^^^^^^^^^^^

Let say we have the directory **/home/Users/tedwaine/xstudio_plugins** added to the Python Plugin Path preference. Within this directory I have another directory called my_plugin. Within that, I could have a file called **demo_plugin.py** that looks like this: 

.. code-block:: python
    :caption: demo_plugin.py - minimal plugin that adds a menu option to the main menu bar

    from xstudio.plugin import PluginBase
    from xstudio.core import KeyboardModifier

    # Declare our plugin class - we're using the PluginBase base class here which
    # is a general purpose plugin
    class MyDemoPlugin(PluginBase):

        def __init__(self, connection):

            PluginBase.__init__(
                self,
                connection,
                "My Demo Plugin")

            self.shortcut_id = self.register_hotkey(
                self.shortcut_callback,
                "W",
                KeyboardModifier.ControlModifier,
                "Demo Plugin Shorcut",
                "Simple example hotkey",
                auto_repeat=False,
                component="Demo Plugins",
                context=None)
            
            self.insert_menu_item(
                "main menu bar",
                "Run Callback",
                "Plugins|Demo Plugin",
                0.0,
                hotkey_uuid=self.shortcut_id,
                callback=self.my_callback,
                user_data="some extra info")

            # ensure that the new 'Plugins' section of the menu bar appears
            # at the end (right side) of the menu bar
            self.set_submenu_position(
                "main menu bar",
                "Plugins",
                1000.0)

        def shortcut_callback(self, shortcut_uuid, context):

            print ("Hotkey pressed in context {}".format(context))

        def my_callback(self):

            print ("my_callback executed")

        def menu_item_activated(self, menu_item_data, user_data):

            print ("my_callback executed with user data", menu_item_data, user_data)

    # This method is required by xSTUDIO
    def create_plugin_instance(connection):
        return MyDemoPlugin(connection)

At the same location in the filesystem I also need an **__init__.py** file to expose the crucial *create_plugin_instance* to xSTUDIO's plugin loader:

.. code-block:: python
    :caption: __init__.py - 

    from .demo_plugin import create_plugin_instance

.. note::
    When developing a python plugin like the example above one can set the XSTUDIO_PYTHON_PLUGIN_PATH to point to your development folder one level up from the folder containing the code of your plugin(s). Simply restart xSTUDIO after making a code change to test. If there are syntax errors or some other runtime problem with importing your plugin the errors will be printed into the terminal. For this reason you must be able to execute xSTUDIO on a command line to make the development cycle fast and easy. There are some hints in the :ref:`Getting Started section <command_line_loading>`.


More Complex plugin Examples
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

**August 2025** - We are still working on some comprehensive Python Plugin API examples that showcase all the features and what is possible in terms of UI interface development, integration and interaction with xSTUDIO. 

However there are some existing plugins that ship with the xSTUDIO source code that provide some fully developed plugins that do HUD overlay graphics or add a custom Viewport layout (picture-in-picture). These plugins are intended to be useful reference implementations of these features and can be found in the **src/plugin/python_plugins/** folder in the xSTUDIO source code repository.
