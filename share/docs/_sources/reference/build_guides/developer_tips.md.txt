# Tips for Developers

### A Faster Development Cycle on Windows

If you are developing xSTUDIO on Windows the build times for both **package** and **install** targets are very long even if you need to test a single line code change. A solution to this problem is to do a full package build first and then copy the contents of the **bin** folder in the target package output folder into the regular **build/bin** folder in the build output folder. This will allow you to run xstudio directly from the **./build/bin** folder after subsequent builds without specifying package or install as the build target. Likewise if your build environment is MS Visual Studio you can set your debug target to **build/bin/xstudio.exe** for a much faster code-change / build / test cycles. The steps to do this in a PowerShell will look something like this:

*Clear the bin and share build output folders (you only need to do this once):*

    rm -r -fo ./bin/build
    rm -r -fo ./bin/share/xstudio

*Copy the corresponding folders from the package output back into the build folder (again, you only need to do this once):*

    cp -r .\build\_CPack_Packages\win64\NSIS\xSTUDIO-1.2.0-win64\bin .\build\
    cp -r .\build\_CPack_Packages\win64\NSIS\xSTUDIO-1.2.0-win64\share\xstudio .\build\share\

*Now you can build and execute xstudio quickly without doing a full package/install build:*

    cmake --build build
    ./bin/xstudio.exe

