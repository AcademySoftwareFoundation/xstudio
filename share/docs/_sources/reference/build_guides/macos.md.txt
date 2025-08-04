## MacOS (Intel/ARM)

To build xSTUDIO on MacOS you must download and install some build dependencies. Once these are in place xSTUDIO can be built with just two commands. Please read these notes carefully, particulary if you are not experienced in software development.

### Download Apple XCode

From the App store, locate and download XCode. Note that it is a large package and may take some time to download. Once downloaded, launch XCode and agree to the license terms & conditions.

### Install 'homebrew' package manager

Some of xSTUDIO's dependencies require 'homebrew', the MacOS open source software package manager. The homepage is [here](https://brew.sh) but you can simply run this command in a terminal 

    /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

We require 5 packages to be installed to proceed. Run these commands in a terminal:

    brew install cmake
    brew install pkg-config
    brew install nasm
    brew install autoconf
    brew install autoconf-archive

### Download and install Qt 6.5.3 SDK

Follow [these instructions](downloading_qt.md)

### Download the VCPKG repo

To build xSTUDIO we need a number of other open source software packages. We use the VCPKG package manager to do this. All that we need to do is download the repo and run the bootstrap script before we build xstudio.

    git clone https://github.com/microsoft/vcpkg.git
    ./vcpkg/bootstrap-vcpkg.sh

### Download the xSTUDIO repo

Download from github in the usual manner. Enter the root folder of the repo and ensure you are building from the correct branch. Example terminal commands might be as follows, to build from the develop branch:

    git clone https://github.com/AcademySoftwareFoundation/xstudio.git
    cd xstudio
    git checkout develop

You must run these commands to add the OpenTimelineIO submodule to the tree and apply a small patch

    git submodule init
    git submodule update
    git apply cmake/otio_patch.diff

### Modify the CMakePresets.json file

Open the CMakePresets.json file (which is in the root of the xstudio repo) in a text editor. You must look for the entry "Qt6_DIR" and modify the value that follows it to point to your installation of the Qt SDK. Specifically, you need to point to a directory named 'Qt6' which is in a directory named 'cmake', which is in a directory named 'lib'. For example, on MacOS where user Mary Jane downloaded Qt into her home folder the entry should look like this:

    "Qt6_DIR": "/Users/maryjane/Qt/6.5.3/macos/lib/cmake/Qt6",

### Build xSTUDIO

Run the appropriate command for your platform (whether you have an older Intel or a newer Apple Silicon machine) to set-up for building. Note that this cmake command ***may take several hours to complete***. This is because xSTUDIO's dependencies (particularly ffmpeg) take a long time to download and build from the source code, which is what VCPKG is doing.

**Apple Silicon (ARM) Machines:**
    
    cmake -B build --preset MacOSRelease

**Intel Machines:**

    cmake -B build --preset MacOSIntelRelease

When this has finished, you can build xSTUDIO with this command. 

    cmake --build build --parallel 16 --target install

If the build is successful, you should have an application bundle in the 'build' folder called 'xSTUDIO.app'. This can be drag & dropped into your applications folder, desktop and dock as for any other application.