.. _playback:

Video Playback
==============


Playheads
---------

.. note::
    The term 'playhead' comes from video playback and editing tools and refers to the indicator on a timeline showing the current on-screen video frame. 

In xSTUDIO, each playlist has its own playhead object meaning that if you switch between playlists the playhead settings such as playback rate, loop mode, compare mode and so-on are remembered *per playlist*. 

.. figure:: ../images/toolbar-01.png
    :align: center
    :alt: The toolbar and transport controls.
    :scale: 90%

    The Playback transport controls.

.. list-table:: Playhead Specific Hotkeys
    :widths: 10 50
    :width: 70 %
    :align: center
    :header-rows: 1
    :stub-columns: 1

    * - Default Keyboard Shortcut
      - Action
    * - Spacebar
      - Start/stop playback.
    * - I
      - Set the 'in' loop point.
    * - O
      - Set the 'out' loop point.
    * - P
      - Toggle in/out loop region or full duration
    * - J
      - Play backwards. Press repeatedly to got into fast rewind.
    * - K
      - Stop playback.
    * - L
      - Play forwards. Press repeatedly to go into fast forward.
    * - Left Arrow
      - Step forwards one frame.
    * - Right Arrow
      - Step backwards one frame.
    * - Down Arrow
      - Step to start of next media item in timeline/String compare mode
    * - Up Arrow
      - Step to start of previous media item in timeline/String compare mode


Playhead Loop Modes
-------------------

Use the loop mode button to switch between 'play once', 'loop' and 'ping-pong' when playing through media.

Playhead Rate
-------------

The playhead rate can be adjusted by clicking and dragging left/right on the 'Rate' button in the toolbar. See :ref:`Transport Controls <_transport>` section. Rate is a simple multiplier applied to the speed that the playhead avances during playback. A value of 0.5 will play your media at half speed, for example. Double click on the button to reset to 1.0.

Playhead Cache Behaviour
------------------------

xSTUDIO will always try to read and decode video data before it is needed for display. The image data is stored in the image cache ready for drawing to screen. xSTUDIO needs to be efficient in how it does this and it is useful if a user understands the behaviour.

If you are not actively playing back media: when you select a new piece of media for viewing, or whenever the position of the playhead is moved, xSTUDIO will start loading images going forward from the playhead position until it has either exhausted the frame range of the clip that you are viewing or until the cache is full. Images that were previously loaded will be deleted from the cache in order to make space for the new images.

The cache status is indicated in the timeline with a horizontal green bar - this should be obvious as you can see it growing as xSTUDIO loads frames in the background. Thus if you want to view media that is slow to read off disk, like high resolution EXR images, the workflow is to wait for xSTUDIO to cache the frames before starting playback/looping. The size of the cache (set via the Preferences panel) will limit the maximum number of frames that can be loaded. 

In most cases xSTUDIO should be able to play back through cached frames at the required frame rate of the media. Although the Viewer has been optimised to get the most out of your graphics card, slow playback can result if you are trying to view very high resolution images and your computer's video hardware can't match the required data transfer rates.

For media that can be decoded faster than the playback rate, like many common compressed video stream codecs or EXRs compressed with the DWA/DWB, you should be able to largely ignore xSTUDIO's cacheing activity as it will be able to stream data off the disk for playback on demand.

Missing Frames Behaviour
------------------------

When xSTUDIO is loading media that is frame based (in other words each video frame is a discreet file on disk like EXR or JPEG files) there are 3 behaviours to choose between when the file corresponding to some frame number that falls in the expected range is *not* on disk. This is important when viewing a CG render, say, where some frames have not yet been rendered but other frames in the frame range are complete. To change the preference go to the File->Preferences->Preference Manager menu option and then go to the 'General' tab in the Preference Manager dialog.

.. figure:: ../images/missing-frames-01.png
    :align: center
    :alt: The missing frames preference
    :scale: 90%

    The missing frames preference

* **Insert Blank Frames** Missing frames are replaced with a blank frame, so the frame range of the image seuquence is preserved. Note that missing/unloadable frames in the transport bar time slider are shown by a red bar in the frame cache indicator
* **Hold Frames** Missing frames are replaced with the previous frame that *could* be loaded. Held frames are indicated with an orange colour bar in the frame cache indicator
* **Skip Missing Frames** With this option the frame range of the Media Source item in question will be collapsed to ignore missing frames. Thus, if you have an image sequence consisting of a sequence of files thus: render_v01.1001.exr, render_v01.1004.exr, render_v01.1011.exr, render_v01.1021.exr then the resulting Mwedia Source item will have a duration of only 4 frames.
