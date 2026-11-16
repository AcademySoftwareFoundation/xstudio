.. _session_log:

##############################
Session Log and Crash Reports
##############################

While it runs, xSTUDIO writes a log of what it is doing. By default the log goes to the terminal only. It can also be written to a file, which is the first thing to look at when something went wrong, and when xSTUDIO crashes it writes a crash report that names that file. This page explains where to find both and how to read them.

The session log
---------------

Every line has the time, the logger name, the level and the message:

.. code-block:: text

    [2026-10-13 11:45:54.730] [xstudio] [info] XStudio logging started.
    [2026-10-13 11:45:56.484] [xstudio] [info] OpenGL: NVIDIA Corporation / NVIDIA GeForce RTX 3070/PCIe/SSE2 / 4.6.0 NVIDIA 580.173.02 (context version 4.6)
    [2026-10-13 11:46:02.118] [xstudio] [warning] media_actor.cpp:1461 Dead Media Source request_receiver_down

The levels are, from quietest to loudest, critical, error, warning, info and debug. The terminal shows info and above; starting xSTUDIO with ``--debug`` shows debug lines as well.

A warning or error raised while xSTUDIO was asking one of its components for something is written as the source file and line where the request was made, followed by the error, as in the third line above. That locates the code for a developer; for a user the message after it is the useful part.

Not everything in the terminal comes from the log. Messages from Qt (the user interface toolkit) and from embedded Python scripts are printed directly and have no time, level or colour, and so is the crash report described below.

Writing the log to a file
-------------------------

Start xSTUDIO with the ``--log-file`` option to write the session log to a file as well as the terminal:

.. code-block:: bash

    xstudio --log-file ~/xstudio.log

The file differs from the terminal in three ways:

- It always receives every level, including debug, whether or not ``--debug`` was given.
- It is written line by line, so it is complete up to the moment of a crash.
- Each run starts a new file. The previous runs are kept next to it with a number added to the name, ``xstudio.log.1`` for the run before, ``xstudio.log.2`` for the one before that, and so on up to ten. A file that grows past 10 MB within one run rolls over in the same way.

The folder is created if it does not exist. If the file cannot be opened, for example because the folder is not writable, an error is logged and xSTUDIO carries on with the terminal only.

.. note::
    Two xSTUDIO instances started with the same ``--log-file`` path write into the same file and roll each other's files over. Give each instance its own path.

Setting the log file in the preferences
---------------------------------------

The log file can also be set with the preference ``/core/logging/log_file``, which is useful when a studio wants every run of xSTUDIO logged without users having to remember the option. When the preference is set it takes precedence over ``--log-file``; when it is empty, the default, the option decides. Environment variables in the value are expanded, so a per-user location can be written as ``${HOME}/xstudio/session.log``.

As with any preference it can be set through a preference file, see :ref:`preferences`. An override file that sets it looks like this:

.. code-block:: json

    {
        "/core/logging/log_file/value": "${HOME}/xstudio/session.log"
    }

The preferences are read a moment after logging starts, so the first few lines of a run, the "logging started" line and any warnings about the preference files themselves, reach the terminal and the ``--log-file`` file but not the file named in the preference. When the preference takes over, the log says so:

.. code-block:: text

    [2026-10-13 11:45:54.815] [xstudio] [info] Session log continues in /home/user/xstudio/session.log

Crash reports
-------------

If xSTUDIO crashes it writes a crash report to the terminal and to a file, then ends immediately with exit status 1. No core dump is produced.

The report file is written to the ``crash`` folder next to the user preferences, ``~/.config/DNEG/xstudio/crash/`` on Linux, with the date, time and process id in its name, for example ``xstudio-crash-20261013-114608-18421.txt``. The last line printed in the terminal gives the full path. When reporting a crash, attach that file together with the session log file it names.

A report is written for these signals: SIGSEGV, SIGABRT (which includes uncaught C++ exceptions and failed assertions), SIGFPE, SIGILL and SIGBUS.

.. note::
    This report is currently available on Linux. On Windows and MacOS xSTUDIO still prints the previous, shorter report (signal number and ten stack frames) for SIGSEGV only, and does not write a file.

.. note::
    xSTUDIO does not run its normal shutdown after a crash. The session log file is complete up to the moment of the crash, because it is written line by line.

What the report contains
^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: text

    xSTUDIO v1.3.0 (git 56844de31, Release) crashed: SIGSEGV (signal 11), address not mapped at 0x10
    time 2026-10-13 11:46:08, pid 18421, thread 18453 "caf.worker", uptime 0h12m34s, resident memory 1890 MB
    host linprec, Linux 5.14.0-687.31.1.el9_8.x86_64 x86_64
    command /opt/xstudio/bin/xstudio.bin --log-file /home/user/xstudio.log /jobs/show/shot/plate.mov
    log file /home/user/xstudio.log
    stack of the crashing thread:
     0: /opt/xstudio/lib/libviewport_ui.so+0x3a51b3 _ZN7xstudio2ui8viewport23ViewportFrameQueueActor12frame_queuedEv+0x1f3
     1: /opt/xstudio/lib/libcaf_core.so.1.0.2+0x114b28 _ZN3caf15scheduled_actor16run_actor_actionsEv+0x2a8
     ...
    Crash report saved to /home/user/.config/DNEG/xstudio/crash/xstudio-crash-20261013-114608-18421.txt

- **The build**: version, git commit and build type, so a developer can match the report to the exact binaries.
- **The signal** and, for memory errors, what went wrong and at which address. A signal sent from outside, for example by ``kill``, is reported with the sending process id instead.
- **Process, thread, uptime and memory use**, host name and operating system, and the command line xSTUDIO was started with.
- **The session log file** in use at the time, or ``none`` when the log went to the terminal only.
- **The stack of the crashing thread**, up to 32 frames. Each line is the module, the offset into the module, the function and the offset into the function.

Reading the stack
^^^^^^^^^^^^^^^^^

Function names are printed as the compiler stores them. To make them readable, pass the report through ``c++filt``:

.. code-block:: bash

    c++filt < xstudio-crash-20261013-114608-18421.txt

If the build has debug information, ``addr2line`` turns a module and offset into a source file and line:

.. code-block:: bash

    addr2line -f -C -i -e /opt/xstudio/lib/libviewport_ui.so 0x3a51b3

Only the thread that crashed is reported. Nothing in the report requires memory allocation, so it is also written when the crash was caused by memory corruption.
