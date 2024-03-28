# Welcome to xSTUDIO

xSTUDIO is a media playback and review application designed for professionals working in the film and TV post production industries, particularly the Visual Effects and Feature Animation sectors. xSTUDIO is focused on providing an intuitive, easy to use interface with a high performance playback engine at its core and C++ and Python APIs for pipeline integration and customisation for total flexibility.

## Building xSTUDIO

This release of xSTUDIO can be built on various Linux flavours. MacOS and Windows compatibility is not available yet but this work is on the roadmap for 2023.

We provide comprehensive build steps for 3 of the most popular Linux distributions.

* [CentOS 7](docs/build_guides/centos_7.md)
* [Rocky Linux 9.1](docs/build_guides/rocky_linux_9_1.md)
* [Ubuntu 22.04](docs/build_guides/ubuntu_22_04.md)

Note that the xSTUDIO user guide is built with Sphinx using the Read-The-Docs theme. The package dependencies for building the docs are somewhat onerous to install and as such we have ommitted these steps from the instructions and instead recommend that you turn off the docs build. Instead, we include the fully built docs (as html pages) as part of this repo and building xSTUDIO will install these pages so that they can be loaded into your browser via the Help menu in the main UI.
