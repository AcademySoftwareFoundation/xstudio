## Linux Generic Guide

xSTUDIO can be built in just a few steps on many Linux distros by employing Microsoft's 'vcpkg' open source package build system. Note that the differences between gcc compiler versions can cause some issues where newer compiler versions are more or less strict about syntax. The developers of xSTUDIO are unable to ensure full compatibility with all compiler variations at this point and it's up to individuals following these guides to work through any issues encountered. Please do submit pull requests if you hit build problems and find solutions!

### Base dependencies

We assume that you have some knowledge of development on Linux platforms and have git, gcc and cmake installed on your system. You also need Ninja, which xSTUDIO uses as its CMake generator on all platforms. Install it via your distro's package manager:

    # Rocky / RHEL / Fedora
    sudo dnf install ninja-build

    # Ubuntu / Debian
    sudo apt install ninja-build

### Download and install Qt 6.5.3 SDK

Follow [these instructions](downloading_qt.md) - ensuring that you download the SDK for Linux platforms (rather than the MacOS platform used as an example in the linked guide).

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
          "name": "LinuxNinjaReleaseLocal",
          "inherits": "LinuxNinjaRelease",
          "cacheVariables": {
            "Qt6_DIR": "/home/maryjane/Qt/6.5.3/gcc_64/lib/cmake/Qt6"
          }
        }
      ]
    }

See the [CMake presets documentation](https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html) for the full format reference.

### Build xSTUDIO

First, we configure for building. Note that this cmake command ***may take several hours to complete*** the first time it is run, though subsequently it will take a few seconds. This is because xSTUDIO's dependencies (particularly ffmpeg) take a long time to download and build from the source code, which is what VCPKG is doing.

    cmake -B build --preset LinuxNinjaReleaseLocal

When this has finished, you can build xSTUDIO with:

    cmake --build build

RelWithDebInfo and Debug variants are also available — see [CMakePresets.json](../../../CMakePresets.json) for the full list.

### Running xSTUDIO in a dev environment

If the compilation is successful you will find the xstudio app in `./build/bin/xstudio.bin`. To enable the Python API, add the built site-packages directory to your `PYTHONPATH`:

    export PYTHONPATH=$PYTHONPATH:$(pwd)/build/bin/python/lib/python3.11/site-packages

Run this from the xstudio repository root. The `python3.11` segment tracks whichever Python version vcpkg built — if you change the `python3` pin in `vcpkg.json`, update this path to match.

### Installing xSTUDIO

Correct packaging and installation of xstudio and its dependencies across various Linux distros is a problem we are still working on! For now, it is up to individual developers to do an effective installation on their system. You can try running `cmake --install build` from the repository root. Use -DCMAKE_BUILD_PREFIX={path} to set a test installation location
