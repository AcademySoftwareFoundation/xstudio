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


Python Examples
===============

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

    # create a playlist
    playlist = XSTUDIO.api.session.create_playlist(name='My Playlist2')

    # make the playlist the 'viewed' playlist 
    XSTUDIO.api.session.set_on_screen_source(playlist[1])

    # add a frames based source (eg. jpegs.0001.jpg, jpegs.0002.jpg etc.)
    frames_based_media = playlist[1].add_media(frames)
    # set the frame rate for the frames source
    frames_based_media.media_source().rate = FrameRate(30.0)

    # add a combined audio/video 
    video_only = playlist[1].add_media(video)

    # add a combined audio/video 
    combined = playlist[1].add_media_with_audio(video, audio)

    # add aan audio only source
    audio_only = playlist[1].add_media(audio)

    # create a playhead for the playlist 
    playlist[1].create_playhead()
    playhead = playlist[1].playheads[0]

    # get the 'playhead_selection' object, which is used to choose
    # items from a playlist for playing 
    plahead_selector = playlist[1].playhead_selection

    # select all the added media for playing (using their uuids) 
    plahead_selector.set_selection(
        [
            frames_based_media.uuid,
            video_only.uuid,
            combined.uuid,
            audio_only.uuid
            ])


    # start playback 
    playhead.playing = True
