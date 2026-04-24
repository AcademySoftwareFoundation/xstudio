## Linux Generic Guide

xSTUDIO can be built in just a few steps on many Linux distros by employing Microsoft's 'vcpkg' open source package build system. Note that the differences between gcc compiler versions can cause some issues where newer compiler versions are more or less strict about syntax. The developers of xSTUDIO are unable to ensure full compatibility with all compiler variations at this point and it's up to individuals following these guides to work through any issues encountered. Please do submit pull requests if you hit build problems and find solutions!

### Base dependencies

We assume that you have some knowledge of development on Linux platforms and have git, gcc & cmake installed on your system.

### Download and install Qt 6.5.3 SDK

Follow [these instructions](downloading_qt.md) - ensuring that you download the SDK for Linux platforms (rather than the MacOS platform used as an example in the linked guide).

### Download the VCPKG repo

To build xSTUDIO we need a number of other open source software packages. We use the VCPKG package manager to do this. All that we need to do is download the repo and run the bootstrap script before we build xstudio. Run these commands in the Powershell:

    git clone https://github.com/microsoft/vcpkg.git
    ./vcpkg/bootstrap-vcpkg.sh

### Download the xSTUDIO repo

Download from github in the usual manner. Enter the root folder of the repo and ensure you are building from the correct branch. Example terminal commands might be as follows, to build from the develop branch:

    git clone https://github.com/AcademySoftwareFoundation/xstudio.git
    cd xstudio
    git checkout develop

### Tell CMake where Qt is installed

CMake needs to know where your Qt 6.5.3 SDK is installed. The simplest way is to set the `Qt6_DIR` environment variable in your shell, before launching the build comman. For example, if user Mary Jane downloaded Qt into her home folder:

    export Qt6_DIR=/home/maryjane/Qt/6.5.3/gcc_64/lib/cmake/Qt6

Optionally, add this to your `~/.bashrc` (or `~/.zshrc`) so it's set in every new shell.

If you prefer not to use an environment variable, alternative options are:

- **Per-machine user preset**: create a `CMakeUserPresets.json` file alongside `CMakePresets.json` in the repo root. The user preset should have a different name from the tracked preset it inherits from, and add `Qt6_DIR` to `cacheVariables`. For example:

        {
          "version": 3,
          "configurePresets": [
            {
              "name": "LinuxReleaseLocal",
              "inherits": "LinuxRelease",
              "cacheVariables": {
                "Qt6_DIR": "/home/maryjane/Qt/6.5.3/gcc_64/lib/cmake/Qt6"
              }
            }
          ]
        }

    See the [CMake presets documentation](https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html) for the full format reference.

- **One-off command-line override**: pass `-DQt6_DIR=/path/to/Qt6` directly to the `cmake` command below.

### Build xSTUDIO

First, we configure for building. Note that this cmake command ***may take several hours to complete*** the first time it is run, though subsequently it will take a few seconds. This is because xSTUDIO's dependencies (particularly ffmpeg) take a long time to download and build from the source code, which is what VCPKG is doing.

- If you are using an environment variable to specify the Qt installation run:

    cmake -B build --preset LinuxRelease

- If you are using a `CMakeUserPresets.json` file to point at your Qt installation (see the previous section), run instead:

    cmake -B build --preset LinuxReleaseLocal

- Or, to pass the Qt path as a one-off command-line override:

    cmake -B build --preset LinuxRelease -DQt6_DIR=/home/maryjane/Qt/6.5.3/gcc_64/lib/cmake/Qt6

When this has finished, you can build xSTUDIO with this command (in this case, the --parallel flag is set for a machine with 16 cores as an example). 

    cmake --build build --parallel 16

### Faster builds with Ninja (optional)

To speed up the build, you can use Ninja instead of make. Ninja parallelises the build at the file level and is noticeably faster. Install it via your platform's package manager:

    # Rocky / RHEL / Fedora
    sudo dnf install ninja-build

    # Ubuntu / Debian
    sudo apt install ninja-build

    # macOS (Homebrew)
    brew install ninja

On Windows, Ninja is already bundled with Visual Studio's CMake tools, so no separate install is needed.

Ninja-based presets are provided for Linux, macOS and Windows (Release, RelWithDebInfo and Debug variants for each). For example, to configure a Linux release build:

    cmake -B build --preset LinuxNinjaRelease
    cmake --build build

Ninja handles parallelism automatically so there is no need for the `--parallel` flag. See [CMakePresets.json](../../../CMakePresets.json) for the full list of available presets.

### Running xSTUDIO in a dev environment

If the compilation is successful you will find the xstudio app in `./build/bin/xstudio.bin`. To enable the Python API, add the built site-packages directory to your `PYTHONPATH`:

    export PYTHONPATH=$PYTHONPATH:$(pwd)/build/bin/python/lib/python3.11/site-packages

Run this from the xstudio repository root. The `python3.11` segment tracks whichever Python version vcpkg built — if you change the `python3` pin in `vcpkg.json`, update this path to match.

### Installing xSTUDIO

Correct packaging and installation of xstudio and its dependencies across various Linux distros is a problem we are still working on! For now, it is up to individual developers to do an effective installation on their system. You can try running 'make install' from the 'build' folder. Use -DCMAKE_BUILD_PREFIX={path} to set a test installation location
