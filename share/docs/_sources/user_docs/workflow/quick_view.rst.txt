.. _quick_view:

QuickView 
=========

VFX and Animation artists may prefer to be able to see individual media items (like their latest playblast render, or whatever) quickly and in isolation, without needing to navigate the xSTUDIO Playlists or Media List. To provide for this sort of workflow feature xSTUDIO has 'QuickView' player windows that is a self-contained, lightweight Viewport instance that just shows a single media item. These windows can be launched very quickly from the main interface, from the command line or from the Python API.

.. note::
    When loading new media with the QuickView feature, it's important that an xSTUDIO instance is **already running**. This allows the QuickView window to be launched very quickly. There must always be a full xSTUDIO instance running somewhere on your desktop to take advantage of the feature.

.. note::
    New media loaded with the quickview feature will always be added to the main xSTUDIO session. This has the advantage that you can always go back and re-view the media item after you have closed the QuickView window.


QuickView from the Command Line
-------------------------------

If you have a terminal/shell shortcut or alias set-up for running xstuio, you can load xSTUDIO into a quickview window easily using the --quick-view command line flag.

.. code-block:: bash
    :caption: QuickView a media item from a terminal (assuming you have a shortcut/alias to the xstudio binary)

    xstudio /Users/jane/data/renders/project_001/shot_f0010/renders --quick-view

.. note::
    Although an xSTUDIO session must already be running for the QuickView feature, the main xSTUDIO window can be minimised or otherwise off screen (on a different desktop workspace, say).

QuickView from the Python API
-----------------------------

For users that want to use xSTUDIO's API for integration with their own tools, and to load media into a quickview window, it is easily done with the Python API.

.. code-block:: python
    :caption: Launch a quickview player

    XSTUDIO = Connection(auto_connect=True)
    import glob
    renders = glob.glob("/Users/joe/reference/water/*wave*.mp4*")
    # load multiple files into a single QuickView using 'Grid' compare
    XSTUDIO.api.session.quickview_media(renders, compare_mode="Grid")

QuickView from the Interface
----------------------------

If desired you can also QuickView media from the interface. In the :ref:`Media List Panel <media_list_panel>` select one or media items and fromthe right-click Context/More menu choose 'QuickView Selected'.

QuickView CLI Demo
^^^^^^^^^^^^^^^^^^

.. raw:: html
    
    <center><video src="../../_static/quick-view-01.webm" width="720" height="366" controls></video></center>

