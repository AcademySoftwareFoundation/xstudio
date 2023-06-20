#################################
Python Scripting Introduction
#################################

xSTUDIO has a python API allowing you to control playback, load media, explore playlists, explore media and more via python scripting. A feature of this API is that behind the scenes all interactions between the Python interpreter and the xSTUDIO application are achieved via a network socket. This means that you can execute python code to control xSTUDIO either in the embedded python interpreter in xSTUDIO or in a completely different process so that your python execution will not block xSTUDIO and vice versa.


The Communications Port
=======================

xSTUDIO opens a port on startup for remote control which is subsequently used by the Python API and also for 'pushing' media to a running session through the command line. The range of port numbers that xSTUDIO will try to open can be specified as a command line option or in the preferences files. When creating a Connection object in Python, you can specify the port number of the xSTUDIO session that you want to connect with. For studios that are interested in managing mulitple instances of xSTUDIO to support a workflow this can be exploited with appopriate wrapper scripts and python integration code

Launch xSTUDIO with a specific port::

    xstudio -n --port=45501

.. code-block:: python
    :caption: Connect to the session

    from urllib.parse import urlparse
    from xstudio.connection import Connection

    XSTUDIO = Connection(auto_connect=False)
    XSTUDIO.connect_remote(host="127.0.0.1", port=45501)


Python 'Snippets'
=================

Some simple python scripts are included with the xSTUDIO source code. These can be executed from the 'Panels/Snippets' menu in the xSTUDIO UI. For developers interested in writing integration tools in python, these snippets can be a useful starting point to get familiar with the API. In the source tree, look in this folder: **python/src/xstudio/demo**

.. note::
    API classes and methods include docstrings - query them using **help()** and **dir()**.


Python Plugins
=================

xSTUDIO now provides a Python plugin API. The features of this API are still being expanded (Q2 2023) but it is ready to be used for some purposes. For example, it's possible to create Viewport overlay graphics, add menu items to some of xSTUDIO's menus that execute callback methods in your plugin. You can create attributes on your python plugin class instance that can be exposed in the QML UI layer to, for example, add buttons to the toolbar or launch fully customised QML interfaces.

To learn more about the plugin API we reccommend that you look at the examples available in the xSTUDIO source repository. These can be found in the **src/demos/python_plugins** subfolder. Note that the dnSetLoopRange is dependent on metadata that is specific to DNEG's pipeline but it is nevertheless commented for 3rd party users to refer to.

To 'install' Python plugins you simply need to copy your plugins to an appropriate location and set the 'XSTUDIO_PYTHON_PLUGIN_PATH' environment variable to point to this location before running the xstudio binary. Typically at a studio sys-admins would advise on this and set-up appropriate wrapper scripts for xSTUDIO that ensure your environment variables are set. However, it's also very straightforward for an individual user. The env var should be a colon separated list of file system paths that include the python-importable module folder of your plugin(s).

For example, let's say you have downloaded the xSTUDIO source repo to the a folder here /home/$USER/Dev/:

.. code-block:: bash
    :caption: Install the demo plugins and launch xSTUDIO to test them 

    sudo mkdir -p /usr/local/lib/xstudio/python-plugins
    cp -r /home/$USER/Dev/xstudio/src/demos/python_plugins/* /usr/local/lib/xstudio/python-plugins/
    export XSTUDIO_PYTHON_PLUGIN_PATH=/usr/local/lib/xstudio/python-plugins
    xstudio


Python Scripting Examples
=========================

Aside from python 'plugins', which are essentially scripts that instance a custom plugin class to handle callbacks and manage certain typical communication with the core app, you also can do many things through straightforward scripts.

**Here's a really basic example that prints out all the media paths in your session.**

.. code-block:: python
    :caption: Print all media paths from current xSTUDIO session

    from urllib.parse import urlparse
    from xstudio.connection import Connection

    XSTUDIO = Connection(auto_connect=True)
    playlists = XSTUDIO.api.session.playlists

    for playlist in playlists:

        media = playlist.media
        for m in media:
            p = urlparse(
                    str(m.media_source().media_reference.uri())
                    )
            print(p.path)


**The following example shows how you can start building a playlist, modify media play rate and start playback:**

.. code-block:: python
    :caption: Adding and playing media basics

    from xstudio.connection import Connection
    from xstudio.core import MediaType, FrameRate

    XSTUDIO = Connection(auto_connect=True)

    # paths to media on the filesystem ...
    frames = '/home/Media/jpeg_sequence.####.jpg'
    video = '/home/Media/some_quicktime.mov'
    audio = '/home/Media/some_audio.wav'


    # create a playlist - note the result is a pair, the first element is the 
    # Uuid, the second is the Playlist class instance
    playlist = XSTUDIO.api.session.create_playlist(name='My Playlist')[1]

    # make the playlist the 'viewed' playlist
    XSTUDIO.api.session.set_on_screen_source(playlist)

    # add a frames based source (eg. jpegs.0001.jpg, jpegs.0002.jpg etc.)
    frames_based_media = playlist.add_media(frames)
    # set the frame rate for the frames source
    frames_based_media.media_source().rate = FrameRate(30.0)

    # add a containerised media item
    video_only = playlist.add_media(video)

    # add a combined audio/video
    combined = playlist.add_media_with_audio(frames, audio)

    # add an audio only source
    audio_only = playlist.add_media(audio)

    # get the playlist's playhead. Each playlist has its own playhead - by making
    # it the 'active' (viewed) playhead below we can start playing media from the 
    # playlist in the viewer.
    playhead = playlist.playhead

    # get the 'playhead_selection' object, which is used to choose
    # items from a playlist for playing
    plahead_selector = playlist.playhead_selection

    # select all the added media for playing (using their uuids)
    plahead_selector.set_selection(
        [
            frames_based_media.uuid,
            video_only.uuid,
            combined.uuid,
            audio_only.uuid
            ])

    XSTUDIO.api.app.set_viewport_playhead(playhead)

    # Some components of xSTUDIO are built with the 'Module' base class. These 
    # components have attributes which can be queried and modified through the API. 

    # To inspect the attributes you can call the attributes_digest() method thus:
    #
    print("Viewport attrs: {0}".format(XSTUDIO.api.app.viewport.attributes_digest(verbose=True).dump(pad=2)))
    print("Playhead attrs: {0}".format(playhead.attributes_digest(True).dump(pad=2)))

    # By inspecting the attributes on the playhead, we can see the compare attr
    # and it's options ... by setting to String the playhead will string together
    # the selected media in a (most basic) sequence. Other options are 'A/B' or 'Off'.
    playhead.set_attribute("Compare", "String")

    # while we're at it let's set the viewport to 'Best' fit, so the whole image is
    # fitted to the viewport area
    XSTUDIO.api.app.viewport.set_attribute("Fit (F)", "Best")

    # start playback
    playhead.playing = True

    # We can also interact with the 'colour_pipeline' object (the OCIO colour 
    # management plugin), for example
    XSTUDIO.api.app.colour_pipeline.set_attribute("Exposure", 0.5)
