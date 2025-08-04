# Welcome to xSTUDIO - v1.0.0

xSTUDIO is a media playback and review application designed for professionals working in the film and TV post production industries, particularly the Visual Effects and Feature Animation sectors. xSTUDIO is focused on providing an intuitive, easy to use interface with a high performance playback engine at its core and C++ and Python APIs for pipeline integration and customisation for total flexibility.

## xSTUDIO Documentation

You can browse xSTUDIO's user and reference documentation [here](share/docs/index.html).

## Building xSTUDIO

This release of xSTUDIO can be built on various Linux flavours, MacOS and Windows 10 and 11. Comprehensive build instructions are available as follows:

* [Windows](docs/reference/build_guides/windows.md)
* [MacOS](docs/reference/build_guides/macos.md)
* [Linux Generic using VCPKG](docs/reference/build_guides/linux_generic.md) 
* [Rocky Linux 9.1](docs/reference/build_guides/rocky_linux_9_1.md)
* [Ubuntu 22.04](docs/reference/build_guides/ubuntu_22_04.md)

### Documentation Note

Note that the xSTUDIO user guide is built with Sphinx using the Read-The-Docs theme and API documentation is auto-generated using the Breathe plugin for Sphinx. The package dependencies for building the docs can be challenging to build/obtain/install so we instead include the fully built docs as part of xSTUDIO's repo, as well as the source for building the docs. Our build set-up by default disables the building of the docs to make life easy!