.. _build_guides:

####################
xSTUDIO Build Guides
####################

At the time of writing these notes (August 2025) xSTUDIO is publically available as *source code* only and as such, to obtain the application, it must be built from the source. 

Post Production Studios and Other Organisations
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Many VFX and Animation studios, especially larger ones, typically have technical teams who have expertise in building software from source code. For such teams with expertise in building and deploying software packages we expect that building xstudio should be straightforward. The build guides below should provide valuable reference. Note that version 1.X of xSTUDIO has been developed against the VFX Reference Platform 2024 standard plus a small set of open source packages/libraries that are easily obtained or built on any platform. A :ref:`list of these build dependencies <dependencies>` is provided for your convenience.

Individual Users
^^^^^^^^^^^^^^^^

**For non-technical individuals we have made it as simple as possible to build xSTUDIO. Try following the appropriate step-by-step guide from the list below.**

If you have any **questions** reach out on the ASWF Slack in the `#ori-xstudio-discussion <https://academysoftwarefdn.slack.com/archives/C07RGJW6MLZ>`_ .

Choosing a guide
^^^^^^^^^^^^^^^^

The **macOS**, **Windows** and **Linux Generic** guides all use the same approach: xSTUDIO's dependencies are built and managed automatically by `vcpkg <https://vcpkg.io>`_, so the only things you need to install by hand are a compiler toolchain, CMake, git and the Qt SDK. These are the **recommended** paths — they work on any reasonably recent distro and require the least manual setup.

The **Rocky Linux 9.1**, **Ubuntu 22.04** and **CentOS 7** guides take a different, more advanced approach: instead of using vcpkg, they resolve xSTUDIO's dependencies from each distro's native package manager (and build a handful of libraries from source when no suitable package exists). This gives a tighter integration with the host system but is significantly more work and more fragile — package names and versions drift over time, and any mismatch against the VFX Reference Platform requires manual intervention. These guides are intended for users who specifically want a native-package build, or whose target distro matches one of the three and who are comfortable troubleshooting package issues. If you just want xSTUDIO to build on a Linux machine, prefer **Linux Generic**.

.. toctree::
    :maxdepth: 1

    macos
    windows
    linux_generic
    rocky_linux_9_1
    ubuntu_22_04
    centos_7
    dependencies