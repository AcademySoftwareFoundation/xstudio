.. _preferences:

###################
xSTUDIO Preferences
###################

Many settings that affect xSTUDIO's components, both in the user interface and in the core of the application and its playback engine, are controlled through the preferences system. The preferences are set-up via the loading of preference files. In general users don't need to be aware of these files and what they are doing, but we include some details here about how those files work and how they can be used to drive customisation of xSTUDIO. This is particularly useful for larger organisations, like post-production studios, that might want to control their own customisations and configuration for xSTUDIO.

The Preferences Manager
-----------------------

Many settings that the user may want control over can be accessed through this interface. It can be launched from the *File->Preferences->Preference Manager* menu from the main menu bar. The user preferences are sorted into categorised Tabs to help with navigation. It can be worthwhile browsing through these settings as they include many customisations that users may wish to take advantage of.

.. figure:: ../images/preferences-01.png
    :alt: The Preferences Manager
    :figclass: align-center
    :align: center
    :scale: 60 %

    The Preferences Manager

.. note::
    Each preference item has a tooltip in the question mark button to describe what it does, often with plenty of detail

Clearing User Preferences
-------------------------

To clear user preferences and restore xSTUDIO to its default configuration you can delete (or better, move so you have still have a backup) the user preference files that xSTUDIO periodically saves. 

.. code-block:: bash
    :caption: Clearing preferences on Linux, MacOS or Windows

    mv  $HOME/.config/DNEG/xstudio  $HOME/.config/DNEG/xstudio_safe


Preference Files (for developers)
---------------------------------

(This section will be more useful for developers or those managing deployment, but there is information that might also be useful for individual users)

xSTUDIO periodically saves the current state of the user preferences and always when the application exits. The location of the user preference files is **$HOME/.config/DNEG/xstudio**. The format of the preference files is **json**. If there is an entry in the *user* preference files, it means the corresponding preference value has been changed from its default value and will be overridden next time xSTUDIO starts up. Default preferences are defined in the files in the **share/preferences** folder that is part of the xSTUDIO distribution (and source code) and browsing these default preference files can be useful to understand how xSTUDIO's configuration can be driven.

Additional preference files can also be loaded when xstudio starts-up via the command line option **--pref**. This means that organisations like post-production studios can override the default xSTUDIO settings using appropriate wrapper scripts and custom preference files.


