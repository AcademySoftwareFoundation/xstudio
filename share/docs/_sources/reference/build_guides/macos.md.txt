## MacOS (Intel/ARM)

To build xSTUDIO on MacOS you must download and install some build dependencies. Once these are in place xSTUDIO can be built with just two commands. Please read these notes carefully, particulary if you are not experienced in software development.

### Download Apple XCode

From the App store, locate and download XCode. Note that it is a large package and may take some time to download. Once downloaded, launch XCode and agree to the license terms & conditions.

### Install 'homebrew' package manager

Some of xSTUDIO's dependencies require 'homebrew', the MacOS open source software package manager. The homepage is [here](https://brew.sh) but you can simply run this command in a terminal 

    /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

We require these packages to be installed to proceed. xSTUDIO uses Ninja as its CMake generator on all platforms, so install it here as well. Run these commands in a terminal:

    brew install cmake
    brew install ninja
    brew install pkg-config
    brew install nasm
    brew install autoconf
    brew install autoconf-archive

### Download and install Qt 6.5.3 SDK

Follow [these instructions](downloading_qt.md)

### Download the VCPKG repo

To build xSTUDIO we need a number of other open source software packages. We use the VCPKG package manager to do this. All that we need to do is download the repo and run the bootstrap script before we build xstudio. Run these commands in a terminal:

    git clone https://github.com/microsoft/vcpkg.git
    git -C vcpkg checkout c2aeddd80357b17592e59ad965d2adf65a19b22f
    ./vcpkg/bootstrap-vcpkg.sh

### Download the xSTUDIO repo

Download from github in the usual manner. Enter the root folder of the repo and ensure you are building from the correct branch. Example terminal commands might be as follows, to build from the develop branch:

    git clone https://github.com/AcademySoftwareFoundation/xstudio.git
    cd xstudio
    git checkout develop

### Tell CMake where Qt is installed

CMake needs to know where your Qt 6.5.3 SDK is installed. Create a `CMakeUserPresets.json` file alongside `CMakePresets.json` in the repo root. This file is gitignored, so your local path won't be committed. The user preset should have a different name from the tracked preset it inherits from, and add `Qt6_DIR` to `cacheVariables`. For example, if user Mary Jane downloaded Qt into her home folder:

    {
      "version": 3,
      "configurePresets": [
        {
          "name": "MacOSNinjaReleaseLocal",
          "inherits": "MacOSNinjaRelease",
          "cacheVariables": {
            "Qt6_DIR": "/Users/maryjane/Qt/6.5.3/macos/lib/cmake/Qt6"
          }
        }
      ]
    }

For an Intel Mac, inherit from `MacOSIntelNinjaRelease` instead. See the [CMake presets documentation](https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html) for the full format reference.

### Build xSTUDIO

Run the configure command. Note that this cmake command ***may take several hours to complete***. This is because xSTUDIO's dependencies (particularly ffmpeg) take a long time to download and build from the source code, which is what VCPKG is doing.

    cmake -B build --preset MacOSNinjaReleaseLocal

When this has finished, you can build xSTUDIO with:

    cmake --build build --target install

RelWithDebInfo and Debug variants are also available — see [CMakePresets.json](../../../CMakePresets.json) for the full list.

If the build is successful, you should have an application bundle in the 'build' folder called 'xSTUDIO.app'. This can be drag & dropped into your applications folder, desktop and dock as for any other application.
