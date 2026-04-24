## MacOS (Intel/ARM)

To build xSTUDIO on MacOS you must download and install some build dependencies. Once these are in place xSTUDIO can be built with just two commands. Please read these notes carefully, particulary if you are not experienced in software development.

### Download Apple XCode

From the App store, locate and download XCode. Note that it is a large package and may take some time to download. Once downloaded, launch XCode and agree to the license terms & conditions.

### Install 'homebrew' package manager

Some of xSTUDIO's dependencies require 'homebrew', the MacOS open source software package manager. The homepage is [here](https://brew.sh) but you can simply run this command in a terminal 

    /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

We require 6 packages to be installed to proceed. Run these commands in a terminal:

    brew install cmake
    brew install pkg-config
    brew install nasm
    brew install autoconf
    brew install autoconf-archive

### Download and install Qt 6.5.3 SDK

Follow [these instructions](downloading_qt.md)

### Download the VCPKG repo

To build xSTUDIO we need a number of other open source software packages. We use the VCPKG package manager to do this. All that we need to do is download the repo and run the bootstrap script before we build xstudio. Run these commands in a terminal:

    git clone https://github.com/microsoft/vcpkg.git
    ./vcpkg/bootstrap-vcpkg.sh

### Download the xSTUDIO repo

Download from github in the usual manner. Enter the root folder of the repo and ensure you are building from the correct branch. Example terminal commands might be as follows, to build from the develop branch:

    git clone https://github.com/AcademySoftwareFoundation/xstudio.git
    cd xstudio
    git checkout develop

### Tell CMake where Qt is installed

CMake needs to know where your Qt 6.5.3 SDK is installed. The simplest way is to set the `Qt6_DIR` environment variable in your shell, pointing to a directory named 'Qt6' which is in a directory named 'cmake', which is in a directory named 'lib'. For example, on MacOS where user Mary Jane downloaded Qt into her home folder:

    export Qt6_DIR=/Users/maryjane/Qt/6.5.3/macos/lib/cmake/Qt6

Add this to your `~/.zshrc` (or `~/.bash_profile`) so it's set in every new shell.

Alternative options if you prefer not to use an environment variable:

- **Per-machine user preset**: create a `CMakeUserPresets.json` file alongside `CMakePresets.json` in the repo root. This file is gitignored, so your local path won't be committed. The user preset should have a different name from the tracked preset it inherits from, and add `Qt6_DIR` to `cacheVariables`. For example:

        {
          "version": 3,
          "configurePresets": [
            {
              "name": "MacOSReleaseLocal",
              "inherits": "MacOSRelease",
              "cacheVariables": {
                "Qt6_DIR": "/Users/maryjane/Qt/6.5.3/macos/lib/cmake/Qt6"
              }
            }
          ]
        }

    See the [CMake presets documentation](https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html) for the full format reference.

- **One-off command-line override**: pass `-DQt6_DIR=/path/to/Qt6` directly to the `cmake` command below.

### Build xSTUDIO

Run the appropriate command for your platform (whether you have an older Intel or a newer Apple Silicon machine) to set-up for building. Note that this cmake command ***may take several hours to complete***. This is because xSTUDIO's dependencies (particularly ffmpeg) take a long time to download and build from the source code, which is what VCPKG is doing.

**Apple Silicon (ARM) Machines:**

    cmake -B build --preset MacOSRelease

**Intel Machines:**

    cmake -B build --preset MacOSIntelRelease

Or, if you are using a `CMakeUserPresets.json` file to point at your Qt installation (see the previous section), run the matching local preset instead, e.g.:

    cmake -B build --preset MacOSReleaseLocal

When this has finished, you can build xSTUDIO with this command. 

    cmake --build build --parallel 16 --target install

If the build is successful, you should have an application bundle in the 'build' folder called 'xSTUDIO.app'. This can be drag & dropped into your applications folder, desktop and dock as for any other application.

### Faster builds with Ninja (optional)

To speed up the build, you can use Ninja instead of make. Ninja parallelises the build at the file level and is noticeably faster. Install it via Homebrew:

    brew install ninja

Ninja-based presets are provided for macOS (Release, RelWithDebInfo and Debug variants). For example, to configure a macOS release build:

    cmake -B build --preset MacOSNinjaRelease
    cmake --build build --target install

Ninja handles parallelism automatically so there is no need for the `--parallel` flag. See [CMakePresets.json](../../../CMakePresets.json) for the full list of available presets.