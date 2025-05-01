# Welcome to xSTUDIO - v1.0.0 (alpha)

xSTUDIO is a media playback and review application designed for professionals working in the film and TV post production industries, particularly the Visual Effects and Feature Animation sectors. xSTUDIO is focused on providing an intuitive, easy to use interface with a high performance playback engine at its core and C++ and Python APIs for pipeline integration and customisation for total flexibility.

This codebase will build version 1.0.0 (alpha) of xstudio. There are some known issues that are currently being worked on:

* Moderate audio distortion on playback (Windows only)
* Ueser Documentation and API Documentation is badly out-of-date.
* Saved sessions might not restore media correctly (Windows only)

## Building xSTUDIO

This release of xSTUDIO can be built on various Linux flavours and Windows 10 and 11. MacOS compatibility is not available yet but this work is on the roadmap for early 2025.

We provide comprehensive build steps for 4 of the most popular distributions.

### Building xSTUDIO for Linux

* [Linux Generic](docs/build_guides/linux_generic.md) 
* [CentOS 7](docs/build_guides/centos_7.md)
* [Rocky Linux 9.1](docs/build_guides/rocky_linux_9_1.md)
* [Ubuntu 22.04](docs/build_guides/ubuntu_22_04.md)

### Building xSTUDIO for Windows

* [Windows](docs/build_guides/windows.md)

### Building xSTUDIO for MacOS

* [MacOS](docs/build_guides/macos.md)

### Documentation Note

Note that the xSTUDIO user guide is built with Sphinx using the Read-The-Docs theme. The package dependencies for building the docs can be challenging to install so we instead include the fully built docs as part of xSTUDIO's repo, as well as the source for building the docs. Our build set-up by default disables the building of the docs to make life easy!