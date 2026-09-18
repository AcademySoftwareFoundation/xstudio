.. _python_scripting:

#############################
Python Scripting Introduction
#############################

xSTUDIO has a python API allowing you to control playback, load media, explore playlists, explore media and more via python scripting. A feature of this API is that behind the scenes all interactions between the Python interpreter and the xSTUDIO application are achieved via a network socket. This means that you can execute python code to control xSTUDIO either in the embedded python interpreter in xSTUDIO or in a completely different process so that your python execution will not block xSTUDIO and vice versa.

Executing Python Code ...
-------------------------

... within the Application
^^^^^^^^^^^^^^^^^^^^^^^^^^

xSTUDIO has an embedded Python interpreter allowing you to run code within the application. Both :ref:`Python snippets <python_snippets>` and :ref:`Python plugins <python_plugins>` provide simple ways to run your custom code within xSTUDIO, as well as the Python interpreter interface panel for interactive execution of Python commands in the application itself.

... remotely
^^^^^^^^^^^^

You can also execute Python code *outside* of xSTUDIO in order to control an instance of the application remotely as described below. In order for this to happen, however, you must be sure that you are running a version of Python that is compatible with the version of Python that xSTUDIO was built with. Also the xSTUDIO python module location should be included on the *sys.path* list.  Better still, if your xSTUDIO installation *includes* Python itself (which is normally the case on MacOS and Windows) then you can run python from the xSTUDIO installation and your script should work without worrying about *sys.path*.

Running a script on Windows could look *something* like this, depending on which version of xSTUDIO you have installed. You can set-up appropriate powershell shortcuts or desktop shortcuts to make this much more convenient.

.. code-block:: bash

    & 'C:\Program Files\xSTUDIO 1.0.0\bin\python3\python.exe' C:\Users\tedwaine\dev\xstudio_scripts\my_script.py

Running a script on MacOS could look *something* like this, depending on which version of xSTUDIO you have installed. You can set-up terminal alias commandas or shortcuts to make this much more convenient.

.. code-block:: bash

    /Applications/xSTUDIO.app/Contents/MacOS/python3.12 C:\Users\tedwaine\dev\xstudio_scripts\my_script.py

On Linux, installation of xSTUDIO is less self-contained than the other distributions and xSTUDIO may have been built with the version of Python on your operating system. As such, you may be able to modify the *PYTHONPATH* environment variable or the *sys.path* at the head of your script to ensure that the xSTUDIO python modules can be found on the Python search path. You can then run scripts directly.


The Communications Port
-----------------------

xSTUDIO opens a port on startup for remote control which is subsequently used by the Python API and also for 'pushing' media to a running session through the command line. The range of port numbers that xSTUDIO will try to open can be specified as a command line option or in the preferences files. When creating a Connection object in Python, you can specify the port number of the xSTUDIO session that you want to connect with. For studios that are interested in managing mulitple instances of xSTUDIO to support a workflow this can be exploited with appopriate wrapper scripts and python integration code

Launch xSTUDIO with a specific port::

    xstudio -n --port=45501

.. code-block:: python
    :caption: Connect to the session

    from urllib.parse import urlparse
    from xstudio.connection import Connection

    XSTUDIO = Connection(auto_connect=False)
    XSTUDIO.connect_remote(host="127.0.0.1", port=45501)

    # now we can interact with xstudio


Python Scripting Examples
-------------------------

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
    combined = playlist.add_media_with_audio(video, audio)

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

    XSTUDIO.api.app.active_viewport.set_playhead(playhead)

    # Some components of xSTUDIO are built with the 'Module' base class. These
    # components have attributes which can be queried and modified through the API.

    # To inspect the attributes you can call the attributes_digest() method thus:
    #
    print("Viewport attrs: {0}".format(XSTUDIO.api.app.active_viewport.list_attributes()))

    # By inspecting the attributes on the playhead, we can see the compare attr
    # and it's options ... by setting to String the playhead will string together
    # the selected media in a (most basic) sequence. Other options are 'A/B' or 'Off'.
    playhead.attributes["Compare"].set_value("String")

    # while we're at it let's set the viewport to 'Best' fit, so the whole image is
    # fitted to the viewport area
    XSTUDIO.api.app.active_viewport.attributes["Fit (F)"].set_value("Best")

    # start playback
    playhead.playing = True

    # We can also interact with the 'colour_pipeline' object (the OCIO colour
    # management plugin), for example
    XSTUDIO.api.app.active_viewport.colour_pipeline.attributes["Exposure (E)"].set_value(0.5)
