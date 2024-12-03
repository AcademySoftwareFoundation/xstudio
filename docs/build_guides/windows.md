# Windows 10/11

* Enable long path support (if you haven't already)
  * Find instructions here: [Maximum File Path Limitation](https://learn.microsoft.com/en-us/windows/win32/fileio/maximum-file-path-limitation?tabs=registry)
* Install git
  * Get it here: [Git Download](https://git-scm.com/download/win)
* Install MS Visual Studio 2022
  * Get it here: [Microsoft Visual Studio](https://visualstudio.microsoft.com/vs/)
  * Ensure CMake tools for Windows is included on install. [CMake projects in Visual Studio](https://learn.microsoft.com/en-us/cpp/build/cmake-projects-in-visual-studio?view=msvc-170#installation)
  * Restart your machine after Visual Studio finishes installing
* Install Ninja (Not required, but highly recommended)
  * Find Ninja here [Ninja Website](https://ninja-build.org/)
* Install Qt 5.15
  * Download the online installer here: [qt.io/download-qt-installer](https://www.qt.io/download-qt-installer-oss)
  * During installation select the following components: ![Qt Components](/docs/build_guides/media/images/Qt5_select_components.png)
    * Qt5.15.2
      * MSVC 2019 64-bit
    * Developer and Designer Tools
      * Qt Creator 10.0.1
      * Qt Creator 10.0.1 CDB Debugger Support
      * Debugging Tools for Windows
  * Note: This can take some time; consider manually [setting a mirror if slow](https://wiki.qt.io/Online_Installer_4.x#Selecting_a_mirror_for_opensource).

* Clone this project to your local drive. Tips to consider:
  * The path should not have spaces in it.
  * Ideally, keep the path short and uncomplicated (IE `D:\xStudio`)
  * Ensure your drive has a decent amount of space free (at least ~40GB)
  * The rest of this document will refer to this location as ${CLONE_ROOT}

* After cloning, from ${CLONE_ROOT} run 'git submodule init'
* Now run 'git submodule update'

* Before loading the project in Visual Studio, consider modifying ${CLONE_ROOT}/CMakePresets.json
  * Edit the `Qt5_DIR` if you did not install in C:\Qt
  * Edit the `CMAKE_INSTALL_PREFIX` to your desired output location
    * This should be outside the build directory in a location where you have permissions to write to.

* Open VisualStudio 2022
  * Use Open Folder to point at the ${CLONE_ROOT}
  * Visual Studio should start configuring the project, including downloading dependencies via VCPKG
    * This process will likely take awhile as it obtains the required dependences.
  * Once configured, you can switch to the Solution Explorer's solution view to view CMake targets.
  * Set your target build to `Release` or `ReleaseWithDeb`
  * Double-click `CMake Targets View`
  * Right-click on `xStudio Project` and select `Build All`
  * Once built, right-click on `xStudio Project` and select `Install`
* If the build succeeds, navigate to your ${CMAKE_INSTALL_PREFIX}/bin and double-click the `xstudio.exe` to run xStudio.


# Questions?

Reach out on the ASWF Slack in the #open-review-initiative channel.
