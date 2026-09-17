.. _web_browser:

The Web Browser Panel
=====================

The Web Browser panel embeds a Chromium based browser (Qt WebEngine) inside
xSTUDIO. It is intended to host web based tools that drive xSTUDIO.

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
is set in Preferences (**Web Browser** tab, **Home Page**).

Anything the page writes to its JavaScript console is forwarded to the
xSTUDIO log, prefixed with ``[web]``, so problems with a page can be read in
the Log panel without opening developer tools.

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
