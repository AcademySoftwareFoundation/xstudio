# Windows 10/11

**These instructions take advantage of a convenient build script that streamlines the whole build process. For an alternative, more granular build steps try [these instructions](windows_old.md)**

## Step 1: Install the build tools

### Enable long path support (if you haven't already)

Find instructions here: [Maximum File Path Limitation](https://learn.microsoft.com/en-us/windows/win32/fileio/maximum-file-path-limitation?tabs=registry)

### Install git

Get it here: [Git Download](https://git-scm.com/download/win)

### Install MS Visual Studio 2022

Get it here: [Microsoft Visual Studio](https://visualstudio.microsoft.com/vs/)
Ensure CMake tools for Windows is included on install. [CMake projects in Visual Studio](https://learn.microsoft.com/en-us/cpp/build/cmake-projects-in-visual-studio?view=msvc-170#installation)
The "CMake tools" component bundles `cmake` and `ninja` (xSTUDIO's CMake generator) along with the MSVC compiler, so no separate install is needed.
Restart your machine after Visual Studio finishes installing.

### Download and install the NSIS tool

NSIS is a packaging system that lets us build xSTUDIO into a Windows installer exe file. Follow the download link on the [NSIS homepage](https://nsis.sourceforge.io/Download). This will download an installer .exe file. Run this program and follow through the steps in the installer wizard with the default installation options until you hit 'Finish'. You can close the NSIS window, it doesn't need to be running for the next steps.

### Prepare a build folder

Start a Windows Powershell to continue these instructions, where you must run a handfull of powershell commands to build xSTUDIO. Windows Powershell is pre-installed, to start it type Powershell into the Search bar in the Start menu - select the 'Run as Administrator' option under the 'Open' button that appears in the options that are offered if you can. 

You will need a location to build xSTUDIO from. We recommend making a folder in your home space, for example, called something like 'dev', as follows:

    mkdir dev
    cd dev

### Download the xSTUDIO repo

Open a Windows PowerShell terminal and navigate (using the 'cd' command) to a suitable location on your file system for building xSTUDIO. Then run these commands.

    git clone https://github.com/AcademySoftwareFoundation/xstudio.git
    cd xstudio

## Step 2: Run Build Script

You can build xSTUDIO by running a single command which executes a script that takes care of all the steps needed to download xSTUDIO's dependencies, build them, and then continue to build xSTUDIO. Note that you may need administrator priveleges to run the script.

To run the build script simply type this command:

    scripts\build_windows.ps1

**N.B. The first time that you run this script expect it to take several hours to complete. This is because it downloads many dependencies of xSTUDIO and builds them from the source code which can take a long time. The build of xSTUDIO itself will take from 2 minutes to 10 minutes, depending on your system's speed and memory.**

On completion a Windows installer .exe file that will install xSTUDIO onto your system will be found at this location:

    ./build/xSTUDIO-1.4.0-win64.exe

## For Developers - Development Cycle and Portable Build (No Installer)

**For your first time build of xSTUDIO follow the instructions above**

Any time you return to doing xSTUDIO devlopment, we assume you will open a new PowerShell terminal. The `cmake` and `ninja` tools, along with the MSVC compiler, are bundled with Visual Studio 2022's "CMake tools" component but are not on your `PATH` by default. Make them available in your PowerShell session by entering the Visual Studio Developer Shell:

    Import-Module "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\Microsoft.VisualStudio.DevShell.dll"
    Enter-VsDevShell -VsInstallPath "C:\Program Files\Microsoft Visual Studio\2022\Community" -Arch amd64 -SkipAutomaticLocation

After running those two commands, `cmake` and `ninja` will resolve directly from the command line for the rest of the session, and the build commands below work as shown.

### Build xSTUDIO

When making and testing code changes, run this command from the root of the xSTUDIO repo to do the build.

    cmake --build build

This builds xSTUDIO without generating the full installer package, and is generally pretty quick. To get the full installer package add **--target package** to the command but note that adding this step is quite slow.

### Running xSTUDIO from the build tree

For a quick dev run without going through the installer, the build generates a launcher at `build/run_xstudio.bat`. Arguments are forwarded to xstudio:

    .\build\run_xstudio.bat path\to\session.xst

### Portable build (no installer)

As an alternative to the NSIS installer you can build a relocatable, no-install folder plus a zip archive:

    cmake --build build --target portable

This produces:

- `build/portable/xSTUDIO-<version>-win64/` - the staged package (kept for inspection), run it via `xstudio.bat` or `bin\xstudio.exe`
- `build/xSTUDIO-<version>-win64-portable.zip` - the same folder as a single archive

Notes:

- Like `--target package`, the `portable` target re-runs the full install including `windeployqt`, so it is about as slow.
- The folder is relocatable, but not data-isolated: preferences, autosaves and thumbnails are still written under the Windows user profile (see the package's `README.txt` for the exact paths).
- `.xst` file associations and Start-menu entries are installer-only and are not part of the portable package.

### Running the unit tests

The tests are not built by default. Add `BUILD_TESTING=ON` when you configure:

    cmake -B build --preset WinNinjaReleaseLocal -DBUILD_TESTING=ON

Build as normal, or build a single test target while you are working on it:

    cmake --build build
    cmake --build build --target helpers_test

Then run the tests with ctest:

    ctest --test-dir build --output-on-failure

Each test is registered as `<component>_<target>`, so `helpers_test` in `src/utility/test` becomes `utility_helpers_test`. You can run a single test with `-R`, and run them in parallel with `-j`:

    ctest --test-dir build --output-on-failure -R utility_helpers_test
    ctest --test-dir build --output-on-failure -j 8

> **Note:** the DLL search path is set up for each test when you configure, so the test executables will run directly from ctest or from your IDE. You do not need to set up an environment first.

Some tests currently fail or time out on Windows, and on Linux too, so a clean run is not expected yet.
