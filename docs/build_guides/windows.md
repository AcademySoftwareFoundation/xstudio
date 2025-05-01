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

You must run these commands to add the OpenTimelineIO submodule to the tree

    git submodule init
    git submodule update

### Build xSTUDIO

Run the first cmake command to set-up for building. Note that this cmake command ***may take several hours to complete***. This is because xSTUDIO's dependencies (particularly ffmpeg) take a long time to download and build from the source code, which is what VCPKG is doing.

First, you may need to find the path to the 'cmake.exe' tool that is part of the VisualStudio install. Substitute as appropriate into the following commands as appropriate.

    'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' -B build --preset WinRelease

When this has finished, you can build xSTUDIO with this command. Note the value after --parallel: change this number to match the number of cores your machine has for best build times.

    'C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' --build build --parallel 16 --target PACKAGE --config Release

If the build is successful, you should have an exectuable in the 'build' folder called something like 'xSTUDIO-1.0.0-win64.exe'. This can be executed to start the xSTUDIO installer.



# Questions?

Reach out on the ASWF Slack in the #open-review-initiative channel.
