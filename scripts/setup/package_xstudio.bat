@echo off
setlocal enabledelayedexpansion

REM Check for help flag
if "%~1"=="-h" (
    echo Usage: package_xstudio.bat [workspacePath] [QTPath] [ffmpegBinFolder]
    echo.
    echo [workspacePath] - The path to your workspace. 
    echo [QTPath] - The path to your QT directory.
    echo [ffmpegBinFolder] - The path to your FFMPEG directory.
    exit /b
)
if "%~1"=="--help" (
    echo Usage: package_xstudio.bat [workspacePath] [QTPath] [ffmpegBinFolder]
    echo.
    echo [workspacePath] - The path to your workspace. 
    echo [QTPath] - The path to your QT directory.
    echo [ffmpegBinFolder] - The path to your FFMPEG directory.
    exit /b
)

REM Command line arguments
set "workspacePath=%~1"
set "QTPath=%~2"
set "ffmpegBinFolder=%~3"

REM Default paths
set "defaultWorkspacePath=C:\workspace\opensource\windows"
set "defaultQTPath=C:\Qt\5.15.2\msvc2019_64"
set "defaultFfmpegBinFolder=C:\ffmpeg\ffmpeg-n5.1-latest-win64-lgpl-shared-5.1\bin"

REM If not provided, use default paths
if not defined workspacePath (
    set /p workspacePath="Please enter the workspace path (default: %defaultWorkspacePath%): "
    if not defined workspacePath set "workspacePath=%defaultWorkspacePath%"
)
if not defined QTPath (
    set /p QTPath="Please enter the QT Path (default: %defaultQTPath%): "
    if not defined QTPath set "QTPath=%defaultQTPath%"
)
if not defined ffmpegBinFolder (
    set /p ffmpegBinFolder="Please enter the FFMPEG Path (default: %defaultFfmpegBinFolder%): "
    if not defined ffmpegBinFolder set "ffmpegBinFolder=%defaultFfmpegBinFolder%"
)

set "buildFolder=%workspacePath%\build\src"
set "debugBinFolder=%workspacePath%\build\bin\Debug"
set "releaseBinFolder=%workspacePath%\build\bin\Release"
set "debugDLLFolder=%workspacePath%\build\vcpkg_installed\x64-windows\debug\bin"
set "releaseDLLFolder=%workspacePath%\build\vcpkg_installed\x64-windows\bin"
set "debugQuickPromiseFolder=%workspacePath%\build\extern\quickpromise\Debug"
set "releaseQuickPromiseFolder=%workspacePath%\build\extern\quickpromise\Release"
set "QTBinFolder=%QTPath%\bin"

REM Create the BIN folder if it doesn't exist
if not exist "%debugBinFolder%" mkdir "%debugBinFolder%"
if not exist "%releaseBinFolder%" mkdir "%releaseBinFolder%"

REM Clear all except xstudio.exe and xstudio.pdb in Debug folder
for %%F in ("%debugBinFolder%\*") do (
    if not "%%~nxF"=="xstudio.exe" (
        if not "%%~nxF"=="xstudio.pdb" (
            del /Q "%%F"
        )
    )
)

REM Clear all except xstudio.exe and xstudio.pdb in Release folder
for %%F in ("%releaseBinFolder%\*") do (
    if not "%%~nxF"=="xstudio.exe" (
        if not "%%~nxF"=="xstudio.pdb" (
            del /Q "%%F"
        )
    )
)

REM Recursively search for "Debug" folders and copy DLL files
for /d /r "%buildFolder%" %%D in (Debug) do (
    pushd "%%D"
    for %%F in (*.dll) do (
        echo Copying "%%~nxF" to "%debugBinFolder%\%%~nxF"
        copy "%%F" "%debugBinFolder%\%%~nxF" /Y
    )
    popd
)

REM Copy all DLL files from release DLL folder
for %%F in ("%releaseDLLFolder%\*.dll") do (
    echo Copying "%%~nxF" to "%debugBinFolder%\%%~nxF"
    copy "%%F" "%debugBinFolder%\%%~nxF" /Y
)

REM Copy all DLL files from debug DLL folder
for %%F in ("%debugDLLFolder%\*.dll") do (
    echo Copying "%%~nxF" to "%debugBinFolder%\%%~nxF"
    copy "%%F" "%debugBinFolder%\%%~nxF" /Y
)

for %%F in ("%debugDLLFolder%\*.pdb") do (
    echo Copying "%%~nxF" to "%debugBinFolder%\%%~nxF"
    copy "%%F" "%debugBinFolder%\%%~nxF" /Y
)


REM Copy all DLL files from QT bin folder to Debug bin folder
for %%F in ("%QTBinFolder%\*.dll") do (
    echo Copying "%%~nxF" to "%debugBinFolder%\%%~nxF"
    copy "%%F" "%debugBinFolder%\%%~nxF" /Y
)

REM Copy all DLL files from QT bin folder to Release bin folder
for %%F in ("%QTBinFolder%\*.dll") do (
    echo Copying "%%~nxF" to "%releaseBinFolder%\%%~nxF"
    copy "%%F" "%releaseBinFolder%\%%~nxF" /Y
)


REM Recursively search for "Release" folders and copy DLL files
for /d /r "%buildFolder%" %%D in (Release) do (
    pushd "%%D"
    for %%F in (*.dll) do (
        echo Copying "%%~nxF" to "%releaseBinFolder%\%%~nxF"
        copy "%%F" "%releaseBinFolder%\%%~nxF" /Y
    )
    popd
)

REM Copy all DLL files from release DLL folder and QT bin folder to Release bin folder
for %%F in ("%releaseDLLFolder%\*.dll" "%QTBinFolder%\*.dll") do (
    echo Copying "%%~nxF" to "%releaseBinFolder%\%%~nxF"
    copy "%%F" "%releaseBinFolder%\%%~nxF" /Y
)

xcopy /E /I "%QTPath%\qml\QtQuick" "%releaseBinFolder%\QtQuick"
xcopy /E /I "%QTPath%\qml\QtGraphicalEffects" "%releaseBinFolder%\QtGraphicalEffects"
xcopy /E /I "%QTPath%\qml\Qt\labs" "%releaseBinFolder%\Qt\labs"
xcopy /E /I "%QTPath%\qml\QtQuick.2" "%releaseBinFolder%\QtQuick.2"
xcopy /E /I "%workspacePath%\build\bin\preference" "%releaseBinFolder%\preference"
xcopy /E /I "%workspacePath%\build\bin\plugin" "%releaseBinFolder%\plugin"
xcopy /E /I "%workspacePath%\build\bin\font" "%releaseBinFolder%\font"

REM Create the QuickPromise folder if it doesn't exist in releaseBinFolder
if not exist "%releaseBinFolder%\QuickPromise" mkdir "%releaseBinFolder%\QuickPromise"

REM Copy DLL files from quickPromiseFolder to the QuickPromise subfolder in releaseBinFolder
for %%F in ("%releaseQuickPromiseFolder%\*.dll") do (
    echo Copying "%%~nxF" to "%releaseBinFolder%\QuickPromise\%%~nxF"
    copy "%%F" "%releaseBinFolder%\QuickPromise\%%~nxF" /Y
)

REM Copy QuickPromise extra files
xcopy /E /I "%workspacePath%\extern\quickpromise\qml" "%releasebinFolder%"

xcopy /E /I "%QTPath%\qml\QtQuick" "%debugBinFolder%\QtQuick"
xcopy /E /I "%QTPath%\qml\QtGraphicalEffects" "%debugBinFolder%\QtGraphicalEffects"
xcopy /E /I "%QTPath%\qml\Qt\labs" "%debugBinFolder%\Qt\labs"
xcopy /E /I "%QTPath%\qml\QtQuick.2" "%debugBinFolder%\QtQuick.2"
xcopy /E /I "%workspacePath%\build\bin\preference" "%debugBinFolder%\preference"
xcopy /E /I "%workspacePath%\build\bin\plugin" "%debugBinFolder%\plugin"
xcopy /E /I "%workspacePath%\build\bin\font" "%debugBinFolder%\font"
set "quickPromiseFolder=%workspacePath%\build\extern\quickpromise\debug"

xcopy /E /I "%QTPath%\qml\QtQuick" "%releaseBinFolder%\QtQuick"
xcopy /E /I "%QTPath%\qml\QtGraphicalEffects" "%releaseBinFolder%\QtGraphicalEffects"
xcopy /E /I "%QTPath%\qml\Qt\labs" "%releaseBinFolder%\Qt\labs"
xcopy /E /I "%QTPath%\qml\QtQuick.2" "%releaseBinFolder%\QtQuick.2"
xcopy /E /I "%workspacePath%\build\bin\preference" "%releaseBinFolder%\preference"
xcopy /E /I "%workspacePath%\build\bin\plugin" "%releaseBinFolder%\plugin"
xcopy /E /I "%workspacePath%\build\bin\font" "%releaseBinFolder%\font"
set "quickPromiseFolder=%workspacePath%\build\extern\quickpromise\release"

REM Create the QuickPromise folder if it doesn't exist in debugBinFolder
if not exist "%debugBinFolder%\QuickPromise" mkdir "%debugBinFolder%\"
if not exist "%releaseBinFolder%\QuickPromise" mkdir "%releaseBinFolder%\"

REM Copy DLL files from quickPromiseFolder to the QuickPromise subfolder in debugBinFolder
for %%F in ("%quickPromiseFolder%\*.dll") do (
    echo Copying "%%~nxF" to "%debugBinFolder%\QuickPromise\%%~nxF"
    copy "%%F" "%debugBinFolder%\QuickPromise\%%~nxF" /Y

    echo Copying "%%~nxF" to "%releaseBinFolder%\QuickPromise\%%~nxF"
    copy "%%F" "%releaseBinFolder%\QuickPromise\%%~nxF" /Y
)

REM Copy QuickPromise extra files
xcopy /E /I "%workspacePath%\extern\quickpromise\qml" "%debugBinFolder%\"
xcopy /E /I "%workspacePath%\extern\quickpromise\qml" "%releaseBinFolder%\"


set "pluginSourceFolder=%workspacePath%\build\src\plugin"
set "debugPluginTargetFolder=%debugBinFolder%\plugin"
set "releasePluginTargetFolder=%releaseBinFolder%\plugin"

REM Create the plugin Target folder if it doesn't exist
if not exist "%debugPluginTargetFolder%" mkdir "%debugPluginTargetFolder%"
if not exist "%releasePluginTargetFolder%" mkdir "%releasePluginTargetFolder%"

REM Recursively search for "Debug" folders under the plugin source directory and copy DLL files
for /d /r "%pluginSourceFolder%" %%D in (Debug) do (
    pushd "%%D"
    for %%F in (*.dll) do (
        echo Copying "%%~nxF" to "%debugPluginTargetFolder%\%%~nxF"
        copy "%%F" "%debugPluginTargetFolder%\%%~nxF" /Y

        echo Copying "%%~nxF" to "%releasePluginTargetFolder%\%%~nxF"
        copy "%%F" "%releasedebugPluginTargetFolder%\%%~nxF" /Y
    )
    popd
)

REM Copy all DLL files from FFMPEG bin folder to Debug bin folder
for %%F in ("%ffmpegBinFolder%\*.dll") do (
    echo Copying "%%~nxF" to "%debugBinFolder%\%%~nxF"
    copy "%%F" "%debugBinFolder%\%%~nxF" /Y
)

REM Copy all DLL files from FFMPEG bin folder to Release bin folder
for %%F in ("%ffmpegBinFolder%\*.dll") do (
    echo Copying "%%~nxF" to "%releaseBinFolder%\%%~nxF"
    copy "%%F" "%releaseBinFolder%\%%~nxF" /Y
)

set "fontsSourceFolder=%workspacePath%\build\bin\fonts"
set "debugFontsTargetFolder=%debugBinFolder%\fonts"
set "releaseFontsTargetFolder=%releaseBinFolder%\fonts"

REM Create the fonts Target folders if they don't exist
if not exist "%debugFontsTargetFolder%" mkdir "%debugFontsTargetFolder%"
if not exist "%releaseFontsTargetFolder%" mkdir "%releaseFontsTargetFolder%"

REM Copy the entire "fonts" directory to "Debug" folder
xcopy /E /I "%fontsSourceFolder%" "%debugFontsTargetFolder%"

REM Copy the entire "fonts" directory to "Release" folder
xcopy /E /I "%fontsSourceFolder%" "%releaseFontsTargetFolder%"

"%QTPath%\bin\windeployqt.exe" --qmldir --force "%buildFolder%" "%releaseBinFolder%\xstudio.exe"
"%QTPath%\bin\windeployqt.exe" "%debugBinFolder%\xstudio.exe"
"%QTPath%\bin\windeployqt.exe" "%releaseBinFolder%\xstudio.exe"
"%QTPath%\bin\windeployqt.exe" --qmldir --force "%buildFolder%" "%debugBinFolder%\xstudio.exe"

echo DLL files from "Debug" folders copied to "%debugBinFolder%"
echo DLL files from "Release" folders copied to "%releaseBinFolder%"
pause
