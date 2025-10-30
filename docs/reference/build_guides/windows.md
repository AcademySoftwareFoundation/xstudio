# Windows 10/11

### Enable long path support (if you haven't already)

Find instructions here: [Maximum File Path Limitation](https://learn.microsoft.com/en-us/windows/win32/fileio/maximum-file-path-limitation?tabs=registry)

### Install git

Get it here: [Git Download](https://git-scm.com/download/win)

### Install MS Visual Studio 2022

Get it here: [Microsoft Visual Studio](https://visualstudio.microsoft.com/vs/)
Ensure CMake tools for Windows is included on install. [CMake projects in Visual Studio](https://learn.microsoft.com/en-us/cpp/build/cmake-projects-in-visual-studio?view=msvc-170#installation)
Restart your machine after Visual Studio finishes installing.

### Download and install Qt 6.5.3 SDK

Follow [these instructions](downloading_qt.md)

### Download and install the NSIS tool

NSIS is a packaging system that lets us build xSTUDIO into a Windows installer exe file. Follow the download link on the [NSIS homepage](https://nsis.sourceforge.io/Download). This will download an installer .exe file. Run this program and follow through the steps in the installer wizard with the default installation options until you hit 'Finish'. You can close the NSIS window, it doesn't need to be running for the next steps.

### Download the VCPKG repo

Start a Windows Powershell to continue these instructions, where you must run a handfull of powershell commands to build xSTUDIO. Windows Powershell is pre-installed, to start it type Powershell into the Search bar in the Start menu. You will need a location to build xSTUDIO from. We recommend making a folder in your home space, called something like 'dev', as follows:

    mkdir dev
    cd dev

To build xSTUDIO we need a number of other open source software packages. We use the VCPKG package manager to do this. All that we need to do is download the repo and run the bootstrap script before we build xstudio. Run these commands in the Powershell:

    git clone https://github.com/microsoft/vcpkg.git
    ./vcpkg/bootstrap-vcpkg.bat

### Download the xSTUDIO repo

Download from github in the usual manner. Enter the root folder of the repo and ensure you are building from the correct branch. Example terminal commands might be as follows, to build from the develop branch:

    git clone https://github.com/AcademySoftwareFoundation/xstudio.git
    cd xstudio
    git checkout develop

You must run these commands to add the OpenTimelineIO submodule to the tree and apply a small patch:

    git submodule init
    git submodule update
    git apply cmake/otio_patch.diff

### Modify the CMakePresets.json file

Open the CMakePresets.json file (which is in the root of the xstudio repo) in a text editor. You must look for the entry "Qt6_DIR" and modify the value that follows it to point to your installation of the Qt SDK. Specifically, you need to point to a directory named 'Qt6' which is in a directory named 'cmake', which is in a directory named 'lib'. For example, on Windows where user Mary Jane downloaded Qt into the root of her C: drive the entry should look like this:

    "Qt6_DIR": ""C:/Qt6/6.5.3/msvc2019_64/lib/cmake/Qt6",

### Build xSTUDIO

Run the first cmake command to set-up for building. Note that this cmake command ***may take several hours to complete***. This is because xSTUDIO's dependencies (particularly ffmpeg) take a long time to download and build from the source code, which is what VCPKG is doing.

First, you may need to find the path to the 'cmake.exe' tool that is part of the VisualStudio install. Substitute as appropriate into the following commands as appropriate.

    'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' -B build --preset WinRelease

When this has finished, you can build xSTUDIO with this command. Note the value after --parallel: change this number to match the number of cores your machine has for best build times.

    'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build build --parallel 16 --target PACKAGE --config Release

If the build is successful, you should have an exectuable in the 'build' folder called something like 'xSTUDIO-1.0.0-win64.exe'. This can be executed to start the xSTUDIO installer.

