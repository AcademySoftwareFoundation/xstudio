# Build XStudio on Windows

## Install basic tools
---

To build XStudio on Windows, you need to install some basic tools. The Windows build uses VCPKG to install third-party dependencies. Follow these steps to set up the development environment:

1. First, ensure that you are using an [administrative](https://www.howtogeek.com/194041/how-to-open-the-command-prompt-as-administrator-in-windows-8.1/) shell.

2. Install MS Visual Studio 2019 (Community Edition is fine) [Visual Studio Legacy Installs](https://visualstudio.microsoft.com/vs/older-downloads/).

3. Restart your machine after Visual Studio finishes installing

4. Execute the script `setup_dev_env.ps1` located in the [script/setup](/scripts/setup/setup_dev_env.ps1) folder.

## Install Qt 5.15
---

To install Qt 5.15, follow these steps:

1. [Download](https://www.qt.io/download-qt-installer-oss?hsCtaTracking=99d9dd4f-5681-48d2-b096-470725510d34%7C074ddad0-fdef-4e53-8aa8-5e8a876d6ab4) the Qt Windows installer.
2. Execute the installer.
3. During the installation, select the components as shown in the following picture:

   ![Qt Components](/docs/build_guides/media/images/Qt5_select_components.png)

4. The installation may take some time. You can grab a cup of coffee while it completes.
5. Note that you will need a valid username and password to download Qt. The installer allows you to add or remove components in the future.

## Install FFMPEG
---
**Important**: The current supported version of FFMPEG is 5.1.

There are two options to install FFMPEG:

1. Using VCPKG.
2. Using a prebuilt binary.

### Install FFMPEG with a prebuilt binary
You can locate prebuilt FFMPEG binaries that fit your licensing criteria [here](https://ffmpeg.org/download.html#build-windows).

Download and extract the archive to your local machine.

## Build XStudio using CMake GUI and Visual Studio 2019
---

To build XStudio using CMake GUI, follow these steps:

1. Configure the CMakePresets.json to your appropriate paths.
   ![Qt5 CMake location](/docs/build_guides/media/images/setup_Qt5.png)
   ![FFMPEG ROOT](/docs/build_guides/media/images/setup_ffmpeg.png)
2. Launch CMake-GUI.
3. Select the source root path in CMake-GUI
4. Select the "windows" preset
5. Select the build location to be in your source root path in a folder called build
6. Click on the "Configure" button.
7. Define Visual Studio 2019 and select the X64 architecture.
8. Click on the "Generate" button.
9. Click on the "Open Project" button.
10. Choose your Build Type (Debug/Release)
11. Select the INSTALL target and build.

# Known Caveats

* Python interpreter is not set up properly
* Audio does not work and may have lots of debugging messages still in-tact.
* Some tools or plugins may not be fully functional.