## Linux Generic Guide

xSTUDIO can be built in just a few steps on many Linux distros by employing Microsoft's 'vcpkg' open source package build system. Note that the differences between gcc compiler versions can cause some issues where newer compiler versions are more or less strict about syntax. The developers of xSTUDIO are unable to ensure full compatibility with all compiler variations at this point and it's up to individuals following these guides to work through any issues encountered. Please do submit pull requests if you hit build problems and find solutions!

### Base dependencies

We assume that you have some knowledge of development on Linux platforms and have git, gcc & cmake installed on your system.

### Download and install Qt 6.5.3 SDK

Follow [these instructions](downloading_qt.md) - ensuring that you download the SDK for Linux platforms (rather than the MacOS platform used as an example in the linked guide).

### Download the VCPKG repo

To build xSTUDIO we need a number of other open source software packages. We use the VCPKG package manager to do this. All that we need to do is download the repo and run the bootstrap script before we build xstudio.

    git clone https://github.com/microsoft/vcpkg.git
    ./vcpkg/bootstrap-vcpkg.sh

### Download the xSTUDIO repo

Download from github in the usual manner. Enter the root folder of the repo and ensure you are building from the correct branch. Example terminal commands might be as follows, to build from the develop branch:

    git clone https://github.com/AcademySoftwareFoundation/xstudio.git
    cd xstudio
    git checkout develop

You must run these commands to add the OpenTimelineIO submodule to the tree

    git submodule init
    git submodule update
    git apply cmake/otio_patch.diff

### Modify the CMakePresets.json file

Open the CMakePresets.json file (which is in the root of the xstudio repo) in a text editor. You must look for the entry "Qt6_DIR" and modify the value that follows it to point to your installation of the Qt SDK. Specifically, you need to point to a directory named 'Qt6' which is in a directory named 'cmake', which is in a directory named 'lib'. For example, if user Mary Jane downloaded Qt into her home folder the entry should look like this:

    "Qt6_DIR": "/home/maryjane/Qt/6.5.3/gcc_64/lib/cmake/Qt6",

### Build xSTUDIO

First, we configure for building. Note that this cmake command ***may take several hours to complete*** the first time it is run, though subsequently it will take a few seconds. This is because xSTUDIO's dependencies (particularly ffmpeg) take a long time to download and build from the source code, which is what VCPKG is doing.
    
    cmake -B build --preset LinuxRelease

When this has finished, you can build xSTUDIO with this command (in this case, the --parallel flag is set for a machine with 16 cores as an example). 

    cmake --build build --parallel 16

### Running xSTUDIO in a dev environment

If the compilation is successfull you will find the xstudio app in ./build/bin/xstudio.bin. To enable the python API, you will need to modify your PYTHONPATH evnironment variable like this, or something similar:

    export PYTHONPATH=$PYTHONPATH:./build/bin/python/lib/python3.10/site-packages

### Installing xSTUDIO

Correct packaging and installation of xstudio and its dependencies across various Linux distros is a problem we are still working on! For now, it is up to individual developers to do an effective installation on their system. You can try running 'make install' from the 'build' folder. Use -DCMAKE_BUILD_PREFIX={path} to set a test installation location
