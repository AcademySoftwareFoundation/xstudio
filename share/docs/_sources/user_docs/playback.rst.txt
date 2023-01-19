##############
Video Playback
##############

Playheads
---------

.. note::
    The term 'playhead' comes from video playback and editing tools and refers to the indicator on a timeline showing the current on-screen video frame. 

In xSTUDIO, each playlist has its own playhead object meaning that if you switch between playlists the playhead settings such as playback rate, loop mode, compare mode and so-on are remembered *per playlist*. To understand more about multi selection of media, please refer to the 'Media List Panel' section.
Playhead 'Compare Modes'

Compare Modes
-------------

The toolbar button labelled 'Compare' sets the compare mode for the current playhead:

    - *A/B:* This allows you to do frame by frame comparisons of more than one media source. xSTUDIO lines up the selected media on the timeline using the frame number or embedded timecode, where available, and decodes and caches frame data for each of the selected sources. **You can then instantaneously cycle through the selection to see one source at a time for a given display frame in the viewport using the up/down arrow keys or the numeric keys.**
    - *String:* This mode simply consecutively strings together the selected media in a continuous timeline. Note that the order of selection of media items is respected (see Media List Panel section for more on the selection order)
    - *Off:* This mode plays one source at a time, whether you have multi-selected several media sources.
    - *Grid, Horizontal, Vertical:* These compare modes are still under development and not available yet, but will allow multiple sources to be viewed on-screen at the same time.

.. note::
    Comparing media in A/B mode makes your computer work harder during playback as it is decoding and storing frames for all of your selected media at the same time. xSTUDIO is optimised to get the most out of your system and in many cases you may be able to A/B compare several sources without imapcting performance, but if you try to A/B compare too many sources at once playback performance may eventually be affected.


Playhead Loop Modes
-------------------

Use the loop mode button to switch between 'play once', 'loop' and 'ping-pong' when playing through media.

Playhead Rate
-------------

The playhead rate can be adjusted by clicking and dragging left/right on the 'Rate' button in the toolbar. This is a simple multiplier applied to the speed that the playhead avances during playback. A value of 0.5 will play your media at half speed, for example. Double click on the button to reset to 1.0.

Playhead Specific Hotkeys
-------------------------

============  ==============================
Shortcut      Action
============  ============================== 
Spacebar      Start/stop playback.
I             Set the 'in' loop point.
O             Set the 'out' loop point.
P             Toggle in/out loop region or full duration
J             Play backwards. Press repeatedly to got into fast rewind.
K             Stop playback.
L             Play forwards. Press repeatedly to go into fast forward.
Left Arrow    Step forwards one frame.
Right Arrow   Step backwards one frame.
============  ============================== 

Playhead Cache Behaviour
------------------------

xSTUDIO will always try to read and decode video data before it is needed for display. The image data is stored in the image cache ready for drawing to screen. xSTUDIO needs to be efficient in how it does this and it is useful if a user understands the behaviour.

If you are not actively playing back media: when you select a new piece of media for viewing, or whenever the position of the playhead is moved, xSTUDIO will start loading images going forward from the playhead position until it has either exhausted the frame range of the clip that you are viewing or until the cache is full. Images that were previously loaded will be deleted from the cache in order to make space for the new images.

The cache status is indicated in the timeline with a horizontal coloured bar - this should be obvious as you can see it growing as xSTUDIO loads frames in the background. Thus if you want to view media that is slow to read off disk, like high resolution EXR images, the workflow is to wait for xSTUDIO to cache the frames before starting playback/looping. The size of the cache (set via the Preferences panel) will limit the maximum number of frames that can be loaded. 

In most cases xSTUDIO should be able to play back through cached frames at the required frame rate of the media. Although the Viewer has been optimised to get the most out of your graphics card, slow playback can result if you are trying to view very high resolution images and your computer's video hardware can't match the required data transfer rates.

For media that can be decoded faster than the playback rate, like many common compressed video stream codecs or EXRs compressed with the DWA/DWB, you should be able to largely ignore xSTUDIO's cacheing activity as it will be able to stream data off the disk for playback on demand.

