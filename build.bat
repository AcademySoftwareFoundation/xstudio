@echo off
setlocal enabledelayedexpansion
REM xSTUDIO Build Script for Windows
REM Usage: build.bat [command] [options]
REM
REM Commands:
REM   configure   - Run CMake configure step only
REM   build       - Build xstudio (runs configure if needed)
REM   deploy      - Copy built binaries to portable\
REM   all         - configure + build + deploy (default)
REM   clean       - Remove build directory
REM   clean-qml   - Delete QML autogen cache (required after QML changes)
REM
REM Options:
REM   --preset <name>   - CMake preset (default: WinRelease)
REM   --config <type>   - Build type: Release|RelWithDebInfo|Debug (default: Release)
REM   --target <name>   - Build target (default: xstudio)
REM   --no-deploy       - Skip deploy step in 'all' command

cd /d "%~dp0"

REM ---------- Defaults ----------
set "BUILD_DIR=build"
set "CONFIG=Release"
set "TARGET=xstudio"
set "COMMAND=all"
set "PRESET="
set "NO_DEPLOY=0"

REM ---------- Parse arguments ----------
:parse_args
if "%~1"=="" goto :end_parse

if /i "%~1"=="configure"  ( set "COMMAND=configure"  & shift & goto :parse_args )
if /i "%~1"=="build"      ( set "COMMAND=build"      & shift & goto :parse_args )
if /i "%~1"=="deploy"     ( set "COMMAND=deploy"     & shift & goto :parse_args )
if /i "%~1"=="all"        ( set "COMMAND=all"        & shift & goto :parse_args )
if /i "%~1"=="clean"      ( set "COMMAND=clean"      & shift & goto :parse_args )
if /i "%~1"=="clean-qml"  ( set "COMMAND=clean-qml"  & shift & goto :parse_args )

if /i "%~1"=="--preset"   ( set "PRESET=%~2"  & shift & shift & goto :parse_args )
if /i "%~1"=="--config"   ( set "CONFIG=%~2"  & shift & shift & goto :parse_args )
if /i "%~1"=="--target"   ( set "TARGET=%~2"  & shift & shift & goto :parse_args )
if /i "%~1"=="--no-deploy" ( set "NO_DEPLOY=1" & shift & goto :parse_args )

if /i "%~1"=="-h"     goto :show_help
if /i "%~1"=="--help" goto :show_help

echo [ERROR] Unknown argument: %~1
exit /b 1

:end_parse

REM ---------- Resolve preset ----------
if "%PRESET%"=="" (
    if /i "%CONFIG%"=="Release"         set "PRESET=WinRelease"
    if /i "%CONFIG%"=="RelWithDebInfo"  set "PRESET=WinRelWithDebInfo"
    if /i "%CONFIG%"=="Debug"           set "PRESET=WinDebug"
    if "%PRESET%"=="" set "PRESET=WinRelease"
)

REM ---------- Dispatch command ----------
if /i "%COMMAND%"=="configure"  goto :do_configure
if /i "%COMMAND%"=="build"      goto :do_build
if /i "%COMMAND%"=="deploy"     goto :do_deploy
if /i "%COMMAND%"=="clean"      goto :do_clean
if /i "%COMMAND%"=="clean-qml"  goto :do_clean_qml
if /i "%COMMAND%"=="all"        goto :do_all
goto :eof

REM ==========================================================
:do_configure
echo [INFO]  Configuring with preset: %PRESET%
cmake --preset %PRESET%
if errorlevel 1 (
    echo [ERROR] Configure failed
    exit /b 1
)
echo [OK]    Configure complete
goto :eof

REM ==========================================================
:do_build
if not exist "%BUILD_DIR%" (
    echo [WARN]  Build directory not found, running configure first...
    call :do_configure
    if errorlevel 1 exit /b 1
)
echo [INFO]  Building target '%TARGET%' (%CONFIG%)...
cmake --build %BUILD_DIR% --config %CONFIG% --target %TARGET%
if errorlevel 1 (
    echo [ERROR] Build failed
    exit /b 1
)
echo [OK]    Build complete
goto :eof

REM ==========================================================
:do_deploy
echo [INFO]  Deploying to portable\...

if not exist "portable\bin" mkdir "portable\bin"
if not exist "portable\share\xstudio\plugin" mkdir "portable\share\xstudio\plugin"

REM --- Main executable ---
if exist "%BUILD_DIR%\bin\%CONFIG%\xstudio.exe" (
    echo   xstudio.exe
    copy /y "%BUILD_DIR%\bin\%CONFIG%\xstudio.exe" "portable\bin\" >nul
) else (
    echo [WARN]  xstudio.exe not found in build output
)

REM --- Core libraries → portable\bin\ ---
set "CORE_PATHS=src\colour_pipeline\src src\module\src"
for %%P in (%CORE_PATHS%) do (
    if exist "%BUILD_DIR%\%%P\%CONFIG%" (
        for %%F in ("%BUILD_DIR%\%%P\%CONFIG%\*.dll") do (
            echo   %%~nxF  [core]
            copy /y "%%F" "portable\bin\" >nul
        )
    )
)

REM --- Plugin libraries → portable\share\xstudio\plugin\ ---
echo [INFO]  Deploying plugins...
set PLUGIN_COUNT=0

REM Walk all plugin subdirectories for Release DLLs (skip test dirs)
for /r "%BUILD_DIR%\src\plugin" %%F in (*.dll) do (
    echo %%F | findstr /i /c:"\test\" >nul
    if errorlevel 1 (
        REM Check this is from a Release (or matching config) directory
        echo %%F | findstr /i /c:"\%CONFIG%\" >nul
        if not errorlevel 1 (
            echo   %%~nxF  [plugin]
            copy /y "%%F" "portable\share\xstudio\plugin\" >nul
            set /a PLUGIN_COUNT+=1
        )
    )
)

echo [OK]    Deployed %PLUGIN_COUNT% plugins
echo [OK]    Deploy complete
goto :eof

REM ==========================================================
:do_clean
echo [INFO]  Removing build directory...
if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
echo [OK]    Clean complete
goto :eof

REM ==========================================================
:do_clean_qml
echo [INFO]  Cleaning QML autogen cache...
if exist "%BUILD_DIR%\src\launch\xstudio\src\xstudio_autogen" (
    rmdir /s /q "%BUILD_DIR%\src\launch\xstudio\src\xstudio_autogen"
)
echo [OK]    QML cache cleaned. Rebuild required.
goto :eof

REM ==========================================================
:do_all
call :do_configure
if errorlevel 1 exit /b 1
call :do_build
if errorlevel 1 exit /b 1
if "%NO_DEPLOY%"=="0" (
    call :do_deploy
    if errorlevel 1 exit /b 1
)
goto :eof

REM ==========================================================
:show_help
for /f "tokens=1,* delims=:" %%a in ('findstr /n "^REM" "%~f0"') do (
    set "line=%%b"
    if defined line (
        set "line=!line:~1!"
        echo !line!
    )
)
exit /b 0
