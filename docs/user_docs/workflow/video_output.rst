.. _video_output:

Video Output (Rendering)
========================

xSTUDIO allows you to render Playlists, Subsets, Contact Sheets and Timelines to a containerised media file (mp4, mov, mkv, webm etc.) using the various video and audio codecs available through the popular `FFMpeg <https://ffmpeg.org/ffmpeg.html>`_ application. You can select one or more items in the Playlists pane to render and then right mouse click to get the context menu (or go via the main File menu) and choose *Export->Render Selected Item(s) to Movie ...*/

.. figure:: ../images/export-to-movie-01.png
    :alt: Export to movie menu
    :figclass: align-center
    :align: center
    :scale: 80%

    Use the context menu or File menu to launch the render wizard.

You will then see get the **Render Video** dialog where you can specify the format of your output video.

.. figure:: ../images/export-to-movie-02.png
    :alt: Render Video dialog
    :figclass: align-center
    :align: center
    :scale: 100%

    The Render Video allows you to fully specify your output video.

What gets Rendered
------------------

What gets rendered depends on the type of item that you have selected according to these simple rules:

    - **Playlistst and Subsets:**  All of the media items within the Playlist or Subset are 'strung' together in a linear sequence reflecting the ordering of the media in the Playlist or Subset. Thus if you have 10 video files in a Playlist the output render will show each of the 10 clips in sequence, and every frame in each video file being used.
    - **Contact Sheets:**  A contact sheet will render exactly as it appears in the xSTUDIO UI, with the same layout (e.g. Grid mode) and the same duration as the corresponding playhead for the source Contact Sheet.
    - **Sequences (Timelines):**  A sequence will render out exactly as it appears in the xSTUDIO Session, so track visibility, clip visibility and the :ref:`Compare Mode <compare_modes>` will also be respected. Thus, for example, you can render out 4 video tracks in a 2x2 arrangement by setting the compare mode to 'Grid' before selecting the sequence for rendering.

.. note::
    Overlay graphics like annotations and HUD items (like media metadata overlays) are included in the renders as long as they are enabled in the main xSTUDIO interface. **Important:** if you switch off the HUD in the xSTUDIO main interface while a render is in-progress then the HUD will also vanish from the render at whatever point the renderer was at when the HUD was disabled. **Therefore, do not adjust the HUD settings while renders are in-progress!**

.. topic:: How do I render a single media item that I have annotated?
    
    Simply create a new :ref:`Subset <subsets>` with just the media item that you're concerned with inside it, select the Subset and execute the *Export->Render Selected Item(s) to Movie ...*/ menu option.

.. topic:: How do I render a sub-range of a single clip?
    
    Create a simple :ref:`Timeline <timeline>` with just the single clip in it. Either trim the clip itself in the timeline to the frame range you want, or use the playhead in/out (Hotkeys **I** and **O**) points to set the loop range to match the frames that you want to render. Now ensure the mini Sequence is selected in the Playlists panel and execute the *Export->Render Selected Item(s) to Movie ...*/ menu option. Now hit the 'Use Loop' button in the Render Video panel to set the frame range for the render (if you used the in/out points to set your loop range in the Timeline).

Render Output Settings
----------------------

.. topic:: Output File

    Choose the file path to render to. Note that the extension of the file will influence the type of container that is created and some containers are not compatible with some video formats. For example, a '.mp4' file cannot contain an Apple ProRes encoded video stream.

.. topic:: Render Range

    This setting only applies to Timelines. You can input the start and end frames to render. Hit the 'Use Full' to set to the full range of the timeline and 'Use Loop' to pick the current loop range on the sequence (if the loop range is indeed set).

.. topic:: Load on Completion

    If this setting is checked xSTUDIO will immediately load the output render when it is complete by adding it to a playlist called **'Render Outputs'** so that you can check your output straight away.

.. topic:: Output Resolution

    Select the resolution for your output from the drop-down list. Note that this list can be extended. Simply click on the **'x'** button in the box to clear the box and then type in a new resolution in the format <x pixels>x<y pixels> (optional name). So for example simple write **1280x720** or **2560x1440 (QHD)**. When you add a new resolution xSTUDIO will store it for future sessions in your user preferences.

.. topic:: Frame Rate

    Select the frame rate for your output. This option does not perform a re-speed when your render frame rate does not match the frame rate of the media being rendered. Instead it performs an accurate video rate 'pull down' mapping. Thus, if your media is 24fps and you render at 48fps then the output clip will play back at visually the same speed as the source where each frame of the source media will be repeated twice in the output render.

.. topic:: Video & Audio Encoding Preset

    Covered in detail below.

.. topic:: Render Audio

    This checkbox allows you to turn off audio and therefore no audio stream is added to the output file.

.. topic:: OCIO Display and OCIO View

    These settings are crucial in configuring the colour management that is applied to the image when it is generated for your encoding. The options in these two drop-downs are dictated by the active OpenColourIO config that applies to the first media item in the Playlist, Subset, Contact Sheet or Sequence that is being rendered. They will generally be initialised to reflect the OCIO settings in the main xSTUDIO interface for the given item to be rendered when it is on-screen. In general, you will get the same RGB pixel values in your output render as you see in the xSTUDIO Viewport when viewing the render target item if you set the OCIO Display and View to match the xSTUDIO Viewport. It is quite straightforward, therefore, to select the colour management settings here and decide which OCIO Display and View you want to be applied to your output render. See the :ref:`Colour Management <colour_management>` section for more details.

    xSTUDIO has the ability for different media items to have different OCIO configs applied but this is generally only applicable to Studios that have set-up pipeline integration plugins that inject the necessary colour managmenet metadata. In the case where you have a Playlist containing media items that have different OCIO configs attached then the Display setting will generally be respected as long as the Display that was selected is valid for the given OCIO config. For a given media item the View setting will then default to the corresponding View setting in the main xSTUDIO interface that you have when the same media item is on-screen.

Executing, Monitoring and Checking a Render
-------------------------------------------

Once you have supplied a filesystem path for your output the 'Render' button will then become clickable and you can dispatch the job to the render queue. Note that you can queue up multiple renders at once - if you have multiple items selected in the Playlist panel the Render Dialog will refresh for the next item when you hit the Render button. Use the 'Skip' button to skip a Playlist item and configure the next selected item for rendering.

.. note::
    The xSTUDIO output rendering feature works entirely in the background and you can continue using the xSTUDIO interface while renders are in progress. On some systems with plenty of RAM, a modern GPU and a high spec CPU then you may not even see any noticeable effect on xSTUDIO's performance while it is rendering and you can continue playing and reviewing media.

.. note::
    When a render is queued, a complete and exact copy of the Playlist, Subset, Contact Sheet or Sequence is created under-the-hood. This means that if you make changes to the Playlist etc. after it has been queued, or even delete it from the session, the render will not be affected.

On hitting the 'Render' button a render status button will appear to the top right of the Playlists panel which looks like this:

.. figure:: ../images/export-to-movie-03.gif
    :alt: Render Video dialog
    :figclass: align-center
    :align: center
    :scale: 100%

    The movie button with animated highlight tells us that xSTUDIO is rendering.

Now, you can click on this button to view the render queue. xSTUDIO will render one output at a time and work through the queue in the order that you dispatched them.

.. figure:: ../images/export-to-movie-04.png
    :alt: Render Queue
    :figclass: align-center
    :align: center
    :scale: 100%

    The render queue interface. Accessed this by clicking on the movie button that appears to the top right of the Playlists panel.

When a job is complete, you can move the mouse over the green tick icon to get a 'play' button. Click on this and a :ref:`Quick View <quick_view>` window will be opened with the rendered output for you to check immediately. Failed renders are indicated by the red warning icon. The button with the monitor hearbeat icon can be clicked to see the full FFMpeg log both during the render and when it's complete or, most usefully, if a render fails. Renders can be cancelled before they start or when they are in progress with the trashcan button, which will also clear successfully completed renders from the queue.

Output Video and Audio Codec Settings
-------------------------------------

How the Renderer Works
^^^^^^^^^^^^^^^^^^^^^^


Before getting into codecs it's useful to know how xSTUDIO's renderer works. During the render xSTUDIO generates image frames using an off-screen Viewport and the image data is written into a virtual file called a 'pipe'. This is done in a background process so that the xSTUDIO interface does not freeze. An instance of the famous media encode/decode application `FFMpeg <https://ffmpeg.org/ffmpeg.html>`_ is also started up by xSTUDIO in the background. This FFMpeg process is set up to receive the images (and audio samples) from xSTUDIO via the pipe. The important thing for the user to know is that the parameters that are passed to FFMpeg when it starts up can be configured by the user via FFMpeg's command line interface. As such, you have total freedom to set-up the video and audio codecs and their tuning parameters used to create the output file - you simply specify the encoding parameters exactly as if you were running FFMpeg in a terminal.

Configuring Encoding Presets
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The Render Video dialog provides a drop-down list of video & audio codec presets. Next to this is a pen icon. Click this to access the video codec presets panel.

.. figure:: ../images/export-to-movie-05.png
    :alt: Render Queue
    :figclass: align-center
    :align: center
    :scale: 100%

    The FFMpeg encode presets panel.

This panel allows you to specify the exact FFMpeg output parameters for the video and audio encoding. The existing presets (which are read-only, and therefore greyed out in the list) can serve as useful reference if you wish to add your own new presets. FFMpeg has an almost limitless number of codec options and special filters that you can supply to tune the video encoding and we do not provide any documentation here. Luckily there are loads of question and answers on the web about how to use and optimise FFMpeg to get particular outputs so it's not too hard to find the settings that you need. Audio encoding options are specified in a separate column and similar principles apply: many encoding settings are possible and it's crucial to set these correctly for successful video generation. Note that if you do not render audio (by unchecking the 'Render Audio' box in the main video render dialog) the audio encode options can be left blank.

There is also a bit depth drop-down setting. This sets the colour resolution of the RGB image that xSTUDIO pipes to FFMpeg - 16 bits is more accurate and is needed if you are outputting a video format like ProRes that has 10 bit or 12 bit bit depth. For many other formats where your output has only 8 bits per channel, selecting 8 bits for the source image will result in much shorter render times without affecting the quality.

.. note::
    An output pixel format (-pix_fmt option) should be specified explicitly in your video encode presets. This is because FFMpeg will otherwise try and pass through the input pixel format (Which is rgba) and this is not compatible with most video encoders.


Troubleshooting 
^^^^^^^^^^^^^^^

If you add new encoding presets and find that your render fails when you use them you will need to debug the settings. Via the Render Queue interface, you can access the FFMpeg output log for the failed render via the heartbeat monitor button. Look very carefully at FFMpeg's log - it can sometimes be hard to spot but you should be able to see what the error was so that you can go back and tweak your presets until the render goes through.





