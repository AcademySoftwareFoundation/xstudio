.. _getting_started:

#####################
Getting Started
#####################

Building xSTUDIO
----------------

Follow our comprehensive :ref:`build guide documentation <build_guides>`: for instructions on building xSTUDIO.


Launching xSTUDIO
-----------------

This depends somewhat on how you have installed xSTUDIO, but generally the method for launching xSTUDIO will be:

* Linux
    On most Linux desktops To launch an empty xSTUDIO "Session" type ``xstudio`` on the Linux command line and press enter.
    Note that *if xstudio is already running* the 'xstudio' command on its own will not launch a new xstudio interface but rather communicate with the exsiting instance. This is how xSTUDIO's fast media *push* loading works. To ensure you get a new xSTUDIO GUI session, you can add the '-n' command line option: ``xstudio -n``.
* Windows
    xSTUDIO installation on Windows should add a shortcut to the Windows start menu.
* MacOS
    xSTUDIO is deployed as an application bundle on MacOS, just double click on the application icon as with any other App.

.. image:: ../images/default-interface.png

Loading Media (filesystem browser)
----------------------------------

An easy way to load media is to drag-and-drop files or folders into the main window from the file system. If you drop a folder, the directory will be recursively searched for media files and they will all be added. 

There are 3 destinations into which you can drag-and-drop from the filesystem browser:

  - The empty space of the Playlists panel : this will create a new playlist and add the media.
    - Subfolders of the top-level folder that was dropped into xSTUDIO will create a *subset*
  - An existing playlist entry in the Playlists panel : this will add the media to the playlist that you dropped the file into.
  - The Media List panel : this will add the media to the end of the playlist being inspected in the media List.

.. raw:: html
    
    <center><video src="../../_static/drag-drop1.webm" width="720" height="366" controls></video></center>

|
    
.. _command_line_loading:

Loading Media (command line)
----------------------------

Media can be loaded using the xSTUDIO command line from a terminal window which will be convenient and powerful for users familiar with shell syntax. By default, if xSTUDIO is already running, files will be added to the existing session instead of starting a new session. If you want to launch a new session, use the -n flag.

xSTUDIO supports various modes for loading sequences. You can mix different modes as required

.. code-block:: bash

    xstudio /path/to/test.mov /path/to/\*.jpg /path/to/frames.####.exr=1-10 /path/to/other_frames.####.exr

On Windows your command might look like this, but by using simple command aliasing in Powershell can simplify this

.. code-block:: bash

    C:\Program Files\xSTUDIO\bin\xstudio.exe C:\Users\JaneSmith\\Media\test_movie.mp4 C:\Users\JaneSmith\Media\exrs\exr_sequence.####.exr

Or, on MacOS you will need the path to the xSTUDIO app bundle, which *may* look like this

.. code-block:: bash

    /Applications/xSTUDIO.app/Contents/MacOS/xstudio.bin /Users/joe/Downloads/imported_media

Note that a specific subset of frames can be loaded, or by ommitting a range all frames matching the pattern will be loaded.

.. note::
     Passing a filesystem directory rather than a filepath means the directory will be recursively searched for media that can be loaded into xSTUDIO.
.. note::
     Movie files will be played back at their 'natural' frame rate, in other words xSTUDIO respects the encoded frame rate of the given file.
.. note::
    Image sequences (e.g. a series of JPEG or EXR files) default to 24fps (you can adjust this in preferences).

For more details on command line loading please see Appendix Pt.1.

Viewing Media
-------------

When you drag/drop into the xSTUDIO media list panel, the first item dropped will be put on screen immediately. To look at other media within the current playlist use the up/down arrow keys to cycle through the items that are in the playlist. See the section 'The Media List Panel' for more ways to select the media that you're viewing. If you drag/drop into the playlists panel, media won't be immediately loaded into the viewport. **You need to double click on the newly created playlist to put its content into the viewport**

.. note::
    The MediaList panel shows the content of the *selected* playlist. Be aware that the *selected playlist* maybe different to the *viewed playlist* that is showing images in the viewer.

To select a playlist single click on it in the Playlist panel. To select a playlist and also start viewing its content, double click the playlist in the Playlist panel.

If your selected playlist is the same as the viewed playlist, simply single click on a media item in the MediaList panel to view it in the viewer and start playing/looping on it. The up/down arrow hotkeys are also an easy way to cycle through the viewed media in the playlist.

If your selected playlist is not the viewed playlist then double clicking on an item in the MediaList will switch your viewed playlist to the selected and the media you clicked on will start playing.

.. _creating_a_sequence:

Creating a Sequence (Timeline)
------------------------------

To start creating a multi track edit you must first have a Playlist because Sequences can only exist as a child of a Playlist (see :ref:`Media List <media_list_panel>` for more details). One can right-mouse-button click with a Playlist selected, or hit the 'More' button (three dots to top right of Playlist panel) and choose *New Sequence*.

Once a Sequence has been created, it can be populated with media by drag-dropping from the parent Playlist or even drag-dropping new media directly into the Media List with the Sequence selected in the Playlists panel. 

.. note::
    Media can be loaded into a Sequence but not be within the edit. In this way the Sequence serves as its own *media bin* as well as containing the actual edit. Media loaded into the sequence will show in the Media List panel, and can be drag and dropped from there into the timeline interface to add to or create new video/audio tracks.

This short video demonstrates Sequence creation.

.. raw:: html
    
    <center><video src="../../_static/building-timeline-01.webm" width="720" height="366" controls></video></center>

