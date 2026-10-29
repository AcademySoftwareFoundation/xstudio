.. _web_browser:

The Web Browser Panel
=====================

The Web Browser panel embeds a Chromium based browser (Qt WebEngine) inside
xSTUDIO. It is intended to host web based tools that drive xSTUDIO, so pages
loaded in it can reach a native bridge object over `Qt WebChannel
<https://doc.qt.io/qt-6/qtwebchannel-index.html>`_; see `The bridge API`_
below.

The panel is only present in builds configured with ``BUILD_WEBENGINE=ON``
(see the build guides). Standard builds do not include it. It is
experimental: off by default, and what it offers may change between releases.

Using the panel
---------------

Pick **Web Browser** from the panel type menu, the same way you would choose
Playlists or Media. The panel has a small toolbar: back, forward, reload (or
stop while a page loads), a URL field, and **Home**. Type a URL into the field
and press Enter to load it. The footer shows the page title, or the load
progress while a page is loading.

The page that opens when the panel is created, and that **Home** returns to,
is set in Preferences (**Web Browser** tab, **Home Page**). The shipped default
is a bundled test page that connects to xSTUDIO over the channel and calls
``ping()``; it prints ``pong`` when everything is wired up.

Anything the page writes to its JavaScript console is forwarded to the
xSTUDIO log, prefixed with ``[web]``, so problems with a page can be read in
the Log panel without opening developer tools.

The bridge API
--------------

The native object is registered on the panel's QWebChannel as
``xstudioWebBridge``. A page reaches it with Qt's ``qwebchannel.js``:

.. code-block:: javascript

    new QWebChannel(qt.webChannelTransport, function (channel) {
        var bridge = channel.objects.xstudioWebBridge;
        bridge.result.connect(function (id, res) { /* res.ok, res.error, ... */ });
        bridge.findPlaylist("Review", function (id) { /* outcome arrives on result(id, res) */ });
    });

``ping()`` and ``version()`` answer directly through the callback. Every other
method returns a request id straight away and does its work on a worker thread,
in call order. ``addMedia`` is the exception: each add appears in the playlist
at once, in call order, but none waits for the one before to load, and each
answers when its file has been read, so a page should fire all its adds at once
and await each ``uuid`` only when it needs it. A file that cannot be opened
still becomes an item, with an error status. Every call adds an item; a page
that wants one item per URL keeps its own map. The outcome arrives on the
``result`` signal with that id and an object that always has ``ok`` and, on
failure, ``error``. Handles are UUID strings. A page gets them from the calls
that create things, and from the lookups (``findPlaylist``, ``findTimeline``)
and the ``list*`` and ``selectedMedia`` calls, which hand out handles to
whatever is already in the session, including containers left by another page
or an earlier run. The bridge only accepts handles it has handed out; a UUID
copied from elsewhere is rejected.

.. list-table::
   :widths: 40 60
   :header-rows: 1

   * - Method
     - Result fields
   * - ``ping()``
     - returns ``"pong"``; the connection check
   * - ``version()``
     - returns ``{xstudio, bridge}``: the release and the bridge API number.
       A web tool can compare these and ask the user for a newer xSTUDIO.
   * - ``findPlaylist(name)``
     - ``uuid``, empty when no playlist has that name.
   * - ``createPlaylist(name)``
     - ``uuid``.
   * - ``setMediaRate(playlistUuid, fps)``
     - Sets the playlist's media rate. Do it before adding media.
   * - ``createTimeline(playlistUuid, name)``
     - ``uuid``. An empty timeline (no default tracks) in that playlist.
   * - ``insertVideoTrack(timelineUuid, name, colour)``
     - ``uuid``. Appends a video track; ``colour`` is ``RRGGBB``, may be empty.
   * - ``insertAudioTrack(timelineUuid, name)``
     - ``uuid``. Appends an audio track.
   * - ``addMedia(playlistUuid, url)``
     - ``uuid``. Adds one media item from ``http(s)://``, ``file://`` or a path.
   * - ``insertClip(trackUuid, mediaUuid, name)``
     - ``uuid``, ``frames``. Appends the media to the track as a clip and
       reports its length.
   * - ``insertGap(trackUuid, frames)``
     - ``uuid``. Appends a gap.
   * - ``showTimeline(timelineUuid)``
     - Makes the timeline the viewed and inspected container.
   * - ``selectLayout(name)``
     - Switches the main window to the named layout ("Review", "Timeline",
       "Present"; empty means "Review").
   * - ``selectPanel(name)``
     - Brings the first tab showing that panel type (for example "Viewport")
       to the front of its tab strip in the current layout; ``ok`` is false
       if the layout has no such tab.
   * - ``createContactSheet(playlistUuid, name)``
     - ``uuid``. A contact sheet in that playlist.
   * - ``addMediaToContactSheet(contactSheetUuid, mediaUuid)``
     - Adds media already in the playlist to the contact sheet; the media is
       shared with the playlist and any timeline, not copied.
   * - ``setViewedContainer(uuid)``
     - Makes a playlist, timeline or contact sheet the viewed and inspected
       container. ``showTimeline`` is this for a timeline.

Reading back. These hand out handles to things the page did not create.

.. list-table::
   :widths: 40 60
   :header-rows: 1

   * - Method
     - Result fields
   * - ``listPlaylists()``
     - ``playlists``: ``[{uuid, name}]``.
   * - ``listContainers(playlistUuid)``
     - ``containers``: ``[{uuid, name, type}]`` where type is ``Timeline``,
       ``ContactSheet`` or ``Subset``. Timelines and contact sheets become
       usable handles; subsets are reported only.
   * - ``listMedia(playlistUuid)``
     - ``media``: ``[{uuid, name, status, flagColour, flagText, uri}]``.
       ``uri`` is the image source's reference as xSTUDIO holds it
       (``file://`` or ``http(s)://``, percent-encoded), empty while the
       item is still being probed.
   * - ``findTimeline(playlistUuid, name)``
     - ``uuid``, empty when the playlist has no timeline of that name.
   * - ``playheadState(timelineUuid)``
     - ``frame``, ``playing``, ``loopIn``, ``loopOut``, ``useLoop`` of the
       timeline's playhead.
   * - ``selectedMedia(playlistUuid)``
     - ``media``: the playlist's selection in order, same entries as
       ``listMedia``.

Playback, on the timeline's playhead.

.. list-table::
   :widths: 40 60
   :header-rows: 1

   * - Method
     - Effect
   * - ``play(timelineUuid, playing)``
     - Starts or stops playback.
   * - ``setFrame(timelineUuid, frame)``
     - Jumps to a frame.
   * - ``setLoopRange(timelineUuid, inFrame, outFrame, enabled)``
     - Sets the loop range and whether it is used.
   * - ``setLoopMode(timelineUuid, mode)``
     - ``"Play Once"``, ``"Loop"`` or ``"Ping Pong"``.
   * - ``setCompareMode(timelineUuid, mode)``
     - The viewer compare mode, for example ``"Off"``, ``"A/B"``, ``"Grid"``.

Editing a timeline the page built.

.. list-table::
   :widths: 40 60
   :header-rows: 1

   * - Method
     - Effect
   * - ``removeClips(trackUuid, index, count, addGap)``
     - Removes ``count`` items from ``index``, leaving a gap if asked.
   * - ``moveClips(trackUuid, start, count, dest)``
     - Moves ``count`` items from ``start`` to index ``dest``.
   * - ``splitClip(trackUuid, index, frame)``
     - Splits the item at ``index`` at a frame within it.
   * - ``clearTimeline(timelineUuid)``
     - Removes every track. Track and clip handles from it are dead after.
   * - ``setItemEnabled(itemUuid, enabled)``
     - Enables or disables a track, clip or gap.
   * - ``setItemName(itemUuid, name)``
     - Renames a track, clip or gap.
   * - ``setItemFlag(itemUuid, colour)``
     - Flags a track, clip or gap with ``RRGGBB``; empty clears the flag.

Media and playlists.

.. list-table::
   :widths: 40 60
   :header-rows: 1

   * - Method
     - Effect
   * - ``setMediaFlag(mediaUuid, colour, text)``
     - Flags a media item with ``RRGGBB`` and a label; empty colour clears.
   * - ``removeMedia(playlistUuid, mediaUuid)``
     - Removes one item from the playlist.
   * - ``clearPlaylist(playlistUuid)``
     - Removes every item from the playlist.
   * - ``removePlaylist(playlistUuid)``
     - Removes the playlist from the session.
   * - ``setSelection(playlistUuid, [mediaUuid])``
     - Sets the playlist's selection, which is what the viewer shows.
   * - ``save()``
     - Saves the session to its current path; fails if it has none.
   * - ``saveAs(path)``
     - Saves the session to a path on the xSTUDIO machine.

Telling the user.

.. list-table::
   :widths: 40 60
   :header-rows: 1

   * - Method
     - Effect
   * - ``notify(kind, text, expiresSeconds)``
     - Shows an xSTUDIO notification; kind is ``"info"``, ``"warn"`` or
       ``"processing"`` (which ignores the expiry).
   * - ``addNote(mediaUuid, subject, text, category, colour, startFrame, durationFrames)``
     - ``uuid``. Adds a note to a media item; ``startFrame`` below zero means
       the whole item.

Each call is one Python API call, nothing more. How a payload turns into
tracks, clips and gaps, and what to show afterwards, is the page's job: one web server is easier
to update than every xSTUDIO install.

Rendering: GPU and CPU
----------------------

By default pages are rendered on the GPU. On Linux with NVIDIA drivers the
default configuration currently produces a **black panel**. This comes from
Qt WebEngine itself: it refuses to use GBM buffer sharing on NVIDIA GPUs,
falls back to rendering in Vulkan, and that fallback does not deliver frames
into xSTUDIO's OpenGL scene graph.

There are two ways around it. Both have only been tried on one machine (Linux,
NVIDIA, Qt 6.10.2, GNOME on Wayland); other GPUs, drivers, Qt versions and a
native X11 session have not been tested.

**GPU rendering on Wayland (recommended).** Qt WebEngine's NVIDIA block can be
bypassed with an environment variable, and on a Wayland session the shared
buffer path then works with the current NVIDIA drivers:

.. code-block:: bash

    export QTWEBENGINE_FORCE_USE_GBM=1
    xstudio

Qt documents this variable as a debugging switch, so it is deliberately not
set by xSTUDIO itself. With ``QT_QPA_PLATFORM=xcb`` under Xwayland the forced
path crashed xSTUDIO on the first web view. A native X11 session has not been
tested.

**CPU rendering.** Turn on **Render Pages on CPU** in the
Web Browser preferences tab, or start xSTUDIO with ``--web-disable-gpu``.
Either passes ``--disable-gpu`` to Chromium. Pages lose WebGL and GPU
compositing, which is fine for tool pages. This is the option to try on X11
sessions, and on any machine where the GPU path misbehaves. It needs a
restart to take effect.

Other Chromium switches can be passed with ``QTWEBENGINE_CHROMIUM_FLAGS``;
xSTUDIO appends ``--disable-gpu`` to whatever is already there when CPU
rendering is requested.

Checking what WebEngine chose
-----------------------------

If a panel is black or blank, run with Qt's WebEngine context logging on and
look at the first lines of output after the panel is created:

.. code-block:: bash

    QT_LOGGING_RULES="qt.webenginecontext.debug=true" xstudio

``Using GBM: yes`` together with ``Using EGL: yes`` is the working GPU
configuration on NVIDIA. ``GBM is not supported with the current
configuration. Fallback to Vulkan rendering in Chromium.`` is the black-panel
configuration; use one of the options above.
