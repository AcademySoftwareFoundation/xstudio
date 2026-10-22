<#
.SYNOPSIS
    End-to-end build automation for xSTUDIO on Windows 10/11.

.DESCRIPTION
    Automates every step of docs/reference/build_guides/windows.md:

      1. Preflight checks    - long path support, git, disk space
      2. Toolchain discovery - Visual Studio 2022 (MSVC + CMake tools), NSIS
      3. Qt 6.5.3            - installed via aqtinstall if not already present
      4. vcpkg               - cloned as a sibling of the repo, pinned + bootstrapped
      5. CMakeUserPresets    - generated with the local Qt6_DIR
      6. VS Developer Shell  - entered so cmake/ninja/cl resolve
      7. Configure + build   - optionally producing the NSIS installer

    The first configure downloads and compiles all vcpkg dependencies
    (ffmpeg, OpenImageIO, OpenColorIO, ...) and MAY TAKE SEVERAL HOURS.

.PARAMETER SourceDir
    xSTUDIO repo root. Defaults to the parent of this script's folder.
    If the folder does not exist it is cloned from GitHub.

.PARAMETER VcpkgRoot
    vcpkg checkout location. Must be a sibling of SourceDir named 'vcpkg',
    because CMakePresets.json hardcodes the toolchain as sourceDir/../vcpkg.

.PARAMETER QtRoot
    Root Qt install directory (the folder that contains '6.5.3'). Default C:\Qt.

.PARAMETER Preset
    Base configure preset from CMakePresets.json. Default WinNinjaRelease.

.PARAMETER Target
    Build target. 'package' builds the NSIS installer, 'all' is a plain build,
    and anything else is forwarded to --target, e.g. a single test target.

    Note: not every test target currently compiles on Windows, so combining
    -BuildTests with -Target all can fail on a test rather than on xSTUDIO
    itself. Build a specific test target when that happens.

.EXAMPLE
    .\scripts\build_windows.ps1
    Full release build plus NSIS installer.

.EXAMPLE
    .\scripts\build_windows.ps1 -Target all -SkipVcpkg -SkipQt
    Fast dev iteration once dependencies are already in place.

.EXAMPLE
    .\scripts\build_windows.ps1 -CheckOnly
    Run preflight/toolchain checks and print a report, build nothing.

.EXAMPLE
    .\scripts\build_windows.ps1 -Target all -RunTests
    Configure with BUILD_TESTING=ON, build, then run the suite through ctest.
    Note that some tests currently fail or time out on Windows, so a non-zero
    ctest result is reported without failing the build.

.EXAMPLE
    .\scripts\build_windows.ps1 -Target all -RunTests -TestFilter utility_helpers_test
    Run a single test.
#>

[CmdletBinding()]
param(
    [string]   $SourceDir,
    [string]   $VcpkgRoot,
    [string]   $QtRoot          = 'C:\Qt',
    [string]   $QtVersion       = '6.5.3',
    [string]   $QtArch          = 'win64_msvc2019_64',
    [string]   $QtDirName       = 'msvc2019_64',

    [ValidateSet('WinNinjaRelease','WinNinjaRelWithDebInfo','WinNinjaDebug',
                 'WinRelease','WinRelWithDebInfo','WinDebug')]
    [string]   $Preset          = 'WinNinjaRelease',

    # 'package' builds the NSIS installer, 'all' is a plain full build, and any
    # other value is passed straight through to --target (e.g. a single test
    # target such as 'helpers_test').
    [string]   $Target          = 'package',

    [string]   $Branch,
    [string]   $VsPath,
    [string]   $MsvcVersion,
    [int]      $Jobs            = 0,
    [string]   $LogDir,

    [string]   $TestFilter,

    [switch]   $BuildTests,
    [switch]   $RunTests,
    [switch]   $EnableLongPaths,
    [switch]   $SkipQt,
    [switch]   $SkipVcpkg,
    [switch]   $SkipConfigure,
    [switch]   $SkipBuild,
    [switch]   $Clean,
    [switch]   $Run,
    [switch]   $CheckOnly
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

# PowerShell 7.4+ turns a non-zero native exit code into a terminating error when
# ErrorActionPreference is 'Stop'. This script probes with commands that are
# *expected* to fail (git config --get, git cat-file -e), and checks $LASTEXITCODE
# explicitly everywhere else, so opt that behaviour out when it exists.
if (Test-Path Variable:PSNativeCommandUseErrorActionPreference) {
    $PSNativeCommandUseErrorActionPreference = $false
}

# vcpkg commit pinned by vcpkg.json "builtin-baseline". Keep the two in sync.
$VCPKG_COMMIT = 'c2aeddd80357b17592e59ad965d2adf65a19b22f'
$XSTUDIO_URL  = 'https://github.com/AcademySoftwareFoundation/xstudio.git'
$VCPKG_URL    = 'https://github.com/microsoft/vcpkg.git'
$MIN_FREE_GB  = 60

# ---------------------------------------------------------------------------
# Output helpers
# ---------------------------------------------------------------------------
$script:StepNo = 0
$script:Report = New-Object System.Collections.ArrayList

function Write-Step {
    param([string]$Message)
    $script:StepNo++
    Write-Host ''
    Write-Host ('=' * 78) -ForegroundColor DarkCyan
    Write-Host ("[{0}] {1}" -f $script:StepNo, $Message) -ForegroundColor Cyan
    Write-Host ('=' * 78) -ForegroundColor DarkCyan
}

function Write-Ok    { param([string]$m) Write-Host "  [ ok ] $m" -ForegroundColor Green }
function Write-Info  { param([string]$m) Write-Host "  [info] $m" -ForegroundColor Gray  }
function Write-Warn2 { param([string]$m) Write-Host "  [warn] $m" -ForegroundColor Yellow }
function Write-Fail  { param([string]$m) Write-Host "  [FAIL] $m" -ForegroundColor Red   }

function Add-Report {
    param([string]$Item, [string]$Status, [string]$Detail = '')
    $null = $script:Report.Add([pscustomobject]@{ Item = $Item; Status = $Status; Detail = $Detail })
}

function Invoke-Native {
    <#  Runs a native executable and throws on a non-zero exit code.
        Native stderr is deliberately NOT redirected - PowerShell 5.1 turns
        that into NativeCommandError noise even for successful runs. #>
    param(
        [Parameter(Mandatory=$true)][string] $Exe,
        [string[]] $Arguments = @(),
        [string]   $WorkDir,
        [string]   $Because
    )
    $shown = "$Exe $($Arguments -join ' ')"
    Write-Info "> $shown"
    if ($WorkDir) { Push-Location $WorkDir }
    try {
        & $Exe @Arguments
        $code = $LASTEXITCODE
    } finally {
        if ($WorkDir) { Pop-Location }
    }
    if ($code -ne 0) {
        $why = $Because
        if (-not $why) { $why = 'command failed' }
        throw "$why (exit $code): $shown"
    }
}

function Test-IsAdmin {
    $id = [Security.Principal.WindowsIdentity]::GetCurrent()
    return ([Security.Principal.WindowsPrincipal]$id).IsInRole(
        [Security.Principal.WindowsBuiltInRole]::Administrator)
}

function ConvertTo-CMakePath {
    param([string]$Path)
    return ($Path -replace '\\', '/')
}

# ---------------------------------------------------------------------------
# Resolve paths
# ---------------------------------------------------------------------------
if (-not $SourceDir) { $SourceDir = Split-Path -Parent $PSScriptRoot }
$SourceDir = [System.IO.Path]::GetFullPath($SourceDir)

if (-not $VcpkgRoot) { $VcpkgRoot = Join-Path (Split-Path -Parent $SourceDir) 'vcpkg' }
$VcpkgRoot = [System.IO.Path]::GetFullPath($VcpkgRoot)

$BuildDir    = Join-Path $SourceDir 'build'
$LocalPreset = "${Preset}Local"

# CMakePresets.json hardcodes CMAKE_TOOLCHAIN_FILE as
# ${sourceDir}/../vcpkg/scripts/buildsystems/vcpkg.cmake. That is only a default:
# the generated CMakeUserPresets.json inherits the tracked preset and can override
# the variable, the same way it overrides Qt6_DIR. So any VcpkgRoot works - we just
# have to emit the override when it is not the sibling directory.
#
# A dedicated vcpkg root per project is often the better choice: vcpkg takes an
# exclusive lock on its root, so projects sharing one checkout serialise against
# each other. The binary cache (%LOCALAPPDATA%\vcpkg\archives) is keyed by ABI hash
# rather than by root, so a separate root still reuses everything already built.
$ExpectedVcpkg    = [System.IO.Path]::GetFullPath((Join-Path (Split-Path -Parent $SourceDir) 'vcpkg'))
$NeedToolchainOverride = ($VcpkgRoot -ne $ExpectedVcpkg)
$VcpkgToolchain   = Join-Path $VcpkgRoot 'scripts\buildsystems\vcpkg.cmake'

if (-not $LogDir) { $LogDir = Join-Path $SourceDir 'build_logs' }
$null = New-Item -ItemType Directory -Force -Path $LogDir
$stamp   = Get-Date -Format 'yyyyMMdd-HHmmss'
$logFile = Join-Path $LogDir "build_windows_$stamp.log"
try { Start-Transcript -Path $logFile -Force | Out-Null }
catch { Write-Host "  [warn] transcript unavailable: $($_.Exception.Message)" -ForegroundColor Yellow }

$sw = [System.Diagnostics.Stopwatch]::StartNew()

Write-Host ''
Write-Host 'xSTUDIO - Windows build automation' -ForegroundColor White
Write-Host "  source : $SourceDir"
Write-Host "  vcpkg  : $VcpkgRoot"
Write-Host "  build  : $BuildDir"
Write-Host "  preset : $LocalPreset (inherits $Preset)"
Write-Host "  target : $Target"
Write-Host "  log    : $logFile"

try {

# ---------------------------------------------------------------------------
Write-Step 'Preflight: long paths, git, disk space'
# ---------------------------------------------------------------------------

$fsKey = 'HKLM:\SYSTEM\CurrentControlSet\Control\FileSystem'
$longPaths = 0
try { $longPaths = (Get-ItemProperty -Path $fsKey -Name LongPathsEnabled -ErrorAction Stop).LongPathsEnabled }
catch { $longPaths = 0 }

if ($longPaths -eq 1) {
    Write-Ok 'NTFS long path support enabled'
    Add-Report 'Long paths' 'ok'
} elseif ($EnableLongPaths) {
    if (-not (Test-IsAdmin)) {
        throw '-EnableLongPaths requires an elevated PowerShell. Re-run as Administrator.'
    }
    Set-ItemProperty -Path $fsKey -Name LongPathsEnabled -Value 1 -Type DWord
    Write-Ok 'Enabled LongPathsEnabled=1 (reboot recommended)'
    Add-Report 'Long paths' 'enabled' 'reboot recommended'
} else {
    Write-Warn2 'Long path support is OFF. vcpkg build trees nest deeply and can fail with path-too-long errors.'
    Write-Warn2 'Re-run this script elevated with -EnableLongPaths, or set LongPathsEnabled=1 manually.'
    Add-Report 'Long paths' 'DISABLED' 'run with -EnableLongPaths (as admin)'
}

$git = Get-Command git -ErrorAction SilentlyContinue
if (-not $git) { throw 'git not found on PATH. Install it from https://git-scm.com/download/win' }
Write-Ok "git: $($git.Source)"
Add-Report 'git' 'ok' $git.Source

# git enforces its own 260-char limit, independent of the NTFS setting above.
$gitLong = & git config --global --get core.longpaths
if ($gitLong -ne 'true') {
    & git config --global core.longpaths true
    Write-Ok 'set git config --global core.longpaths true'
}

$driveLetter = Split-Path -Qualifier $SourceDir
$disk   = Get-CimInstance Win32_LogicalDisk -Filter "DeviceID='$driveLetter'"
$freeGb = [math]::Round($disk.FreeSpace / 1GB, 1)
if ($freeGb -lt $MIN_FREE_GB) {
    Write-Warn2 "Only ${freeGb}GB free on $driveLetter. vcpkg needs roughly ${MIN_FREE_GB}GB for sources plus build trees."
    Add-Report 'Disk space' 'LOW' "${freeGb}GB free on $driveLetter"
} else {
    Write-Ok "${freeGb}GB free on $driveLetter"
    Add-Report 'Disk space' 'ok' "${freeGb}GB free"
}

# ---------------------------------------------------------------------------
Write-Step 'Locate Visual Studio 2022 (MSVC + CMake tools)'
# ---------------------------------------------------------------------------

if (-not $VsPath) {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path $vswhere)) {
        throw "vswhere.exe not found at '$vswhere'. Install Visual Studio 2022 from https://visualstudio.microsoft.com/vs/"
    }
    $VsPath = & $vswhere -latest -products * `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        -property installationPath
    if (-not $VsPath) {
        throw 'No Visual Studio install with the MSVC x64 toolset was found. Add the "Desktop development with C++" workload.'
    }
    $VsPath = $VsPath.Trim()
}
if (-not (Test-Path $VsPath)) { throw "Visual Studio path does not exist: $VsPath" }
Write-Ok "Visual Studio: $VsPath"
Add-Report 'Visual Studio' 'ok' $VsPath

$devShellDll = Join-Path $VsPath 'Common7\Tools\Microsoft.VisualStudio.DevShell.dll'
if (-not (Test-Path $devShellDll)) {
    throw "Microsoft.VisualStudio.DevShell.dll not found under '$VsPath'."
}

# xSTUDIO's Windows audio backend includes <atlbase.h>, so the toolset actually
# selected must ship ATL. Several MSVC toolsets can be installed side by side and
# only some carry atlmfc; worse, the dev shell defaults to the version named in
# Microsoft.VCToolsVersion.v143.default.txt, which is not necessarily the one in
# Microsoft.VCToolsVersion.default.txt that vcpkg picks up. Left alone that builds
# the dependencies with one toolset and xSTUDIO with another, and only fails ~an
# hour in with 'Cannot open include file: atlbase.h'. Choose explicitly instead.
$toolsetRoot = Join-Path $VsPath 'VC\Tools\MSVC'
$toolsets = @(Get-ChildItem $toolsetRoot -Directory -ErrorAction SilentlyContinue |
              Sort-Object { [version]$_.Name } -Descending)
$withAtl = @($toolsets | Where-Object { Test-Path (Join-Path $_.FullName 'atlmfc\include\atlbase.h') })

if ($MsvcVersion) {
    $chosen = $toolsets | Where-Object { $_.Name -eq $MsvcVersion } | Select-Object -First 1
    if (-not $chosen) {
        throw ("-MsvcVersion '$MsvcVersion' is not installed. Available: " +
               (($toolsets | ForEach-Object { $_.Name }) -join ', '))
    }
    if ($chosen.FullName -and -not (Test-Path (Join-Path $chosen.FullName 'atlmfc\include\atlbase.h'))) {
        Write-Warn2 "toolset $MsvcVersion has no ATL headers - the audio backend will fail to compile."
    }
} elseif ($withAtl.Count -gt 0) {
    $chosen = $withAtl[0]
} else {
    throw ("No installed MSVC toolset provides ATL (atlmfc\include\atlbase.h), which xSTUDIO's " +
           "Windows audio backend requires. Installed: " +
           (($toolsets | ForEach-Object { $_.Name }) -join ', ') + ". Add the " +
           '"C++ ATL for latest v143 build tools (x86 & x64)" component in the Visual Studio Installer.')
}

$MsvcVersion = $chosen.Name
Write-Ok "MSVC toolset: $MsvcVersion (ATL present)"
Add-Report 'MSVC toolset' 'ok' $MsvcVersion
foreach ($ts in $toolsets) {
    if ($ts.Name -ne $MsvcVersion) {
        $has = Test-Path (Join-Path $ts.FullName 'atlmfc\include\atlbase.h')
        Write-Info "  also installed: $($ts.Name) (ATL: $has)"
    }
}

# ---------------------------------------------------------------------------
Write-Step 'Check NSIS (needed only for --target package)'
# ---------------------------------------------------------------------------

$nsis = $null
$nsisCandidates = @(
    (Join-Path ${env:ProgramFiles(x86)} 'NSIS\makensis.exe'),
    (Join-Path $env:ProgramFiles 'NSIS\makensis.exe')
)
foreach ($c in $nsisCandidates) {
    if (Test-Path $c) { $nsis = $c; break }
}
if (-not $nsis) {
    $cmd = Get-Command makensis.exe -ErrorAction SilentlyContinue
    if ($cmd) { $nsis = $cmd.Source }
}
if ($nsis) {
    Write-Ok "NSIS: $nsis"
    Add-Report 'NSIS' 'ok' $nsis
} elseif ($Target -eq 'package') {
    Write-Warn2 'NSIS not found - CPack would fail at the very end of a multi-hour build.'
    Write-Warn2 'Install it from https://nsis.sourceforge.io/Download, or build with -Target all.'
    Add-Report 'NSIS' 'MISSING' 'required by -Target package'
} else {
    Write-Info 'NSIS not found (not required for -Target all)'
    Add-Report 'NSIS' 'n/a' 'not needed for -Target all'
}

# ---------------------------------------------------------------------------
Write-Step "Qt $QtVersion ($QtDirName)"
# ---------------------------------------------------------------------------

$qtPrefix = Join-Path (Join-Path $QtRoot $QtVersion) $QtDirName
$qt6Dir   = Join-Path $qtPrefix 'lib\cmake\Qt6'

if ($SkipQt) {
    Write-Info 'skipped (-SkipQt)'
} elseif ($CheckOnly -and -not (Test-Path $qt6Dir)) {
    Write-Warn2 "not installed at $qtPrefix (-CheckOnly: not installing)"
} elseif (Test-Path $qt6Dir) {
    Write-Ok "already installed: $qtPrefix"
} else {
    Write-Info "not found at $qtPrefix - installing with aqtinstall"

    $py = $null
    foreach ($c in @('py','python','python3')) {
        $cmd = Get-Command $c -ErrorAction SilentlyContinue
        if ($cmd) { $py = $cmd.Source; break }
    }
    if (-not $py) {
        throw ("Qt $QtVersion is not installed and Python was not found, so aqtinstall cannot run. " +
               "Install Python 3 from https://www.python.org/downloads/ and re-run, or install Qt " +
               "$QtVersion manually (MSVC 2019 64-bit plus Qt Image Formats) and pass -QtRoot.")
    }
    Write-Info "python: $py"

    Invoke-Native -Exe $py -Arguments @('-m','pip','install','--upgrade','aqtinstall') `
                  -Because 'pip install aqtinstall failed'
    Invoke-Native -Exe $py -Arguments @('-m','aqt','install-qt','windows','desktop',
                                        $QtVersion,$QtArch,'-O',$QtRoot,'-m','qtimageformats') `
                  -Because 'aqt install-qt failed'

    if (-not (Test-Path $qt6Dir)) {
        throw "aqtinstall finished but '$qt6Dir' is missing. Check -QtArch / -QtDirName."
    }
    Write-Ok "installed: $qtPrefix"
}

if (Test-Path $qt6Dir) {
    Add-Report "Qt $QtVersion" 'ok' $qtPrefix
} else {
    Add-Report "Qt $QtVersion" 'MISSING' $qtPrefix
    if (-not $CheckOnly) { throw "Qt6 CMake package not found at '$qt6Dir'." }
}

# ---------------------------------------------------------------------------
Write-Step 'xSTUDIO repository'
# ---------------------------------------------------------------------------

if (Test-Path (Join-Path $SourceDir 'CMakePresets.json')) {
    Write-Ok "using existing checkout: $SourceDir"
} elseif ($CheckOnly) {
    Write-Warn2 "no checkout at $SourceDir (-CheckOnly: not cloning)"
    Add-Report 'xSTUDIO repo' 'MISSING' $SourceDir
    $script:Report | Format-Table -AutoSize
    exit 0
} else {
    Write-Info "cloning $XSTUDIO_URL -> $SourceDir"
    Invoke-Native -Exe 'git' -Arguments @('clone',$XSTUDIO_URL,$SourceDir) -Because 'xstudio clone failed'
}
if ($Branch) {
    Invoke-Native -Exe 'git' -Arguments @('-C',$SourceDir,'checkout',$Branch) -Because "checkout $Branch failed"
    Write-Ok "on branch $Branch"
}
$headRef = (& git -C $SourceDir rev-parse --abbrev-ref HEAD).Trim()
$headSha = (& git -C $SourceDir rev-parse --short HEAD).Trim()
Write-Ok "HEAD: $headRef @ $headSha"
Add-Report 'xSTUDIO repo' 'ok' "$headRef @ $headSha"

# ---------------------------------------------------------------------------
Write-Step "vcpkg (pinned to $($VCPKG_COMMIT.Substring(0,12)))"
# ---------------------------------------------------------------------------

if ($CheckOnly) {
    # -CheckOnly reports, it does not clone, check out, or bootstrap anything.
    if (Test-Path (Join-Path $VcpkgRoot 'vcpkg.exe')) {
        $at = (& git -C $VcpkgRoot rev-parse --short HEAD).Trim()
        if ((& git -C $VcpkgRoot rev-parse HEAD).Trim() -eq $VCPKG_COMMIT) {
            Write-Ok "bootstrapped and on the pinned baseline ($at)"
            Add-Report 'vcpkg' 'ok' $VcpkgRoot
        } else {
            Write-Warn2 "bootstrapped but at $at, not the pinned $($VCPKG_COMMIT.Substring(0,12))"
            Add-Report 'vcpkg' 'wrong commit' $at
        }
    } else {
        Write-Warn2 "not present at $VcpkgRoot (-CheckOnly: not cloning)"
        Add-Report 'vcpkg' 'MISSING' $VcpkgRoot
    }
} elseif ($SkipVcpkg) {
    Write-Info 'skipped (-SkipVcpkg)'
    if (Test-Path (Join-Path $VcpkgRoot 'vcpkg.exe')) {
        Add-Report 'vcpkg' 'skipped' $VcpkgRoot
    } else {
        Write-Warn2 "-SkipVcpkg was passed but '$VcpkgRoot\vcpkg.exe' does not exist - configure will fail."
        Add-Report 'vcpkg' 'MISSING' $VcpkgRoot
    }
} else {
    if (-not (Test-Path (Join-Path $VcpkgRoot '.git'))) {
        Write-Info "cloning $VCPKG_URL -> $VcpkgRoot"
        Invoke-Native -Exe 'git' -Arguments @('clone',$VCPKG_URL,$VcpkgRoot) -Because 'vcpkg clone failed'
    }

    $current = (& git -C $VcpkgRoot rev-parse HEAD).Trim()
    if ($current -ne $VCPKG_COMMIT) {
        Write-Info "checking out pinned baseline (was $($current.Substring(0,12)))"
        & git -C $VcpkgRoot cat-file -e "$VCPKG_COMMIT^{commit}"
        if ($LASTEXITCODE -ne 0) {
            Invoke-Native -Exe 'git' -Arguments @('-C',$VcpkgRoot,'fetch','--all','--tags') -Because 'vcpkg fetch failed'
        }
        Invoke-Native -Exe 'git' -Arguments @('-C',$VcpkgRoot,'checkout',$VCPKG_COMMIT) -Because 'vcpkg checkout failed'
    }
    Write-Ok "at $VCPKG_COMMIT"

    $vcpkgExe = Join-Path $VcpkgRoot 'vcpkg.exe'
    if (-not (Test-Path $vcpkgExe)) {
        Write-Info 'bootstrapping vcpkg'
        Invoke-Native -Exe (Join-Path $VcpkgRoot 'bootstrap-vcpkg.bat') -WorkDir $VcpkgRoot `
                      -Because 'bootstrap-vcpkg.bat failed'
    }
    if (-not (Test-Path $vcpkgExe)) { throw "vcpkg.exe missing after bootstrap: $vcpkgExe" }
    Write-Ok "bootstrapped: $vcpkgExe"
    Add-Report 'vcpkg' 'ok' $VcpkgRoot
}

# ---------------------------------------------------------------------------
Write-Step "CMakeUserPresets.json ($LocalPreset)"
# ---------------------------------------------------------------------------

$userPresetPath = Join-Path $SourceDir 'CMakeUserPresets.json'
$qt6Cmake       = ConvertTo-CMakePath $qt6Dir

if ($CheckOnly) {
    Write-Info "would write $LocalPreset (inheriting $Preset) with Qt6_DIR = $qt6Cmake"
    if ($NeedToolchainOverride) {
        Write-Info "  and CMAKE_TOOLCHAIN_FILE = $(ConvertTo-CMakePath $VcpkgToolchain)"
    }
    Add-Report 'CMakeUserPresets' 'not written' '-CheckOnly'

    Write-Step 'Check summary (-CheckOnly: nothing was built or modified)'
    $script:Report | Format-Table -AutoSize
    exit 0
}

$presets = @()
if (Test-Path $userPresetPath) {
    $existing = Get-Content -Raw -Path $userPresetPath | ConvertFrom-Json
    if ($existing.PSObject.Properties.Name -contains 'configurePresets') {
        # Drop any earlier version of our preset, keep the user's other entries.
        $presets = @($existing.configurePresets | Where-Object { $_.name -ne $LocalPreset })
    }
    Copy-Item $userPresetPath "$userPresetPath.bak" -Force
    Write-Info 'backed up existing file to CMakeUserPresets.json.bak'
}

$cacheVars = [ordered]@{ Qt6_DIR = $qt6Cmake }
if ($NeedToolchainOverride) {
    $cacheVars['CMAKE_TOOLCHAIN_FILE'] = ConvertTo-CMakePath $VcpkgToolchain
}

$presets += [pscustomobject]@{
    name           = $LocalPreset
    inherits       = $Preset
    cacheVariables = [pscustomobject]$cacheVars
}

$doc = [pscustomobject]@{ version = 3; configurePresets = @($presets) }
$doc | ConvertTo-Json -Depth 12 | Set-Content -Path $userPresetPath -Encoding utf8
Write-Ok "Qt6_DIR = $qt6Cmake"
if ($NeedToolchainOverride) {
    Write-Ok "CMAKE_TOOLCHAIN_FILE = $(ConvertTo-CMakePath $VcpkgToolchain)"
}
Add-Report 'CMakeUserPresets' 'ok' $LocalPreset

# vcpkg roots each keep their own downloads/ of source tarballs. Point them all at
# one directory so a dedicated root does not re-download several GB that another
# root already has. (The binary cache of built packages is already global.)
if (-not $env:VCPKG_DOWNLOADS) {
    $sharedDownloads = Join-Path $env:LOCALAPPDATA 'vcpkg\downloads'
    $null = New-Item -ItemType Directory -Force -Path $sharedDownloads
    $env:VCPKG_DOWNLOADS = $sharedDownloads
    Write-Info "VCPKG_DOWNLOADS = $sharedDownloads (shared tarball cache)"
}

# ---------------------------------------------------------------------------
Write-Step 'Enter Visual Studio Developer Shell'
# ---------------------------------------------------------------------------

if ($env:VSCMD_VER) {
    Write-Ok "already inside a VS dev shell (VSCMD_VER=$env:VSCMD_VER)"
} else {
    Import-Module $devShellDll
    # -vcvars_ver pins the toolset chosen above, overriding the v143 default.
    Enter-VsDevShell -VsInstallPath $VsPath -Arch amd64 -SkipAutomaticLocation `
                     -DevCmdArguments "-vcvars_ver=$MsvcVersion" | Out-Null
    Write-Ok "entered dev shell (VSCMD_VER=$env:VSCMD_VER, toolset $MsvcVersion)"
}

$clPath = (Get-Command cl -ErrorAction SilentlyContinue).Source
if ($clPath -and $clPath -notmatch [regex]::Escape($MsvcVersion)) {
    Write-Warn2 "cl.exe resolved to '$clPath', which is not the selected toolset $MsvcVersion."
}

# Enter-VsDevShell does NOT add the bundled CMake/Ninja to PATH, so 'ninja' can
# resolve to an unrelated copy that happens to sit earlier on PATH (Strawberry
# Perl ships one, next to a MinGW gcc/ar that CMake and vcpkg probes can then
# pick up and misconfigure the toolchain with). Put the VS-bundled tools first.
$vsCMakeBin = Join-Path $VsPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin'
$vsNinjaBin = Join-Path $VsPath 'Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja'
foreach ($dir in @($vsCMakeBin, $vsNinjaBin)) {
    if (Test-Path $dir) {
        $env:PATH = "$dir;$env:PATH"
        Write-Info "prepended to PATH: $dir"
    } else {
        Write-Warn2 ("VS-bundled tools not found at '$dir' - falling back to whatever is on PATH. " +
                     'Add the "C++ CMake tools for Windows" component in the Visual Studio Installer.')
    }
}

$required = @('cmake','cl')
if ($Preset -like '*Ninja*') { $required += 'ninja' }
foreach ($tool in $required) {
    $found = Get-Command $tool -ErrorAction SilentlyContinue
    if (-not $found) {
        throw ("'$tool' is not available after entering the dev shell. Add the " +
               '"C++ CMake tools for Windows" component in the Visual Studio Installer.')
    }
    Write-Ok "$tool -> $($found.Source)"
    Add-Report $tool 'ok' $found.Source
}

# ---------------------------------------------------------------------------
Write-Step 'Configure'
# ---------------------------------------------------------------------------

if ($Clean -and (Test-Path $BuildDir)) {
    Write-Warn2 "removing build directory: $BuildDir"
    Remove-Item -Recurse -Force $BuildDir
}

# A CMakeCache.txt records the absolute directory it was generated in. If the
# tree was moved (or copied from another machine/drive), CMake aborts with a
# fairly opaque error - catch it here and say exactly what to do instead.
$cacheFile = Join-Path $BuildDir 'CMakeCache.txt'
if (-not $Clean -and (Test-Path $cacheFile)) {
    $cachedHome = Select-String -Path $cacheFile -Pattern '^CMAKE_HOME_DIRECTORY:INTERNAL=(.*)$' |
                  Select-Object -First 1
    if ($cachedHome) {
        $cachedDir = $cachedHome.Matches[0].Groups[1].Value
        $sameTree  = ((ConvertTo-CMakePath $cachedDir).TrimEnd('/') -ieq
                      (ConvertTo-CMakePath $SourceDir).TrimEnd('/'))
        if (-not $sameTree) {
            throw ("Stale build directory: '$cacheFile' was generated for source tree " +
                   "'$cachedDir', but this run's source tree is '$SourceDir'. CMake cannot " +
                   "reuse a relocated cache. Re-run with -Clean to delete '$BuildDir' and " +
                   'configure from scratch.')
        }
    }
}

if ($Jobs -gt 0) {
    $env:VCPKG_MAX_CONCURRENCY = "$Jobs"
    Write-Info "VCPKG_MAX_CONCURRENCY=$Jobs"
}

# vcpkg takes an exclusive lock on its root, so a build driven from another
# project against the same vcpkg checkout blocks this one. vcpkg waits quietly
# ('waiting to take filesystem lock...'), which looks exactly like slow progress
# - name the process holding it up front instead.
$otherVcpkg = @(Get-CimInstance Win32_Process -Filter "Name='vcpkg.exe'" -ErrorAction SilentlyContinue |
                Where-Object { $_.CommandLine -and $_.CommandLine -match [regex]::Escape((ConvertTo-CMakePath $VcpkgRoot)) })
if ($otherVcpkg.Count -gt 0) {
    Write-Warn2 "$($otherVcpkg.Count) vcpkg.exe process(es) are already using '$VcpkgRoot':"
    foreach ($p in $otherVcpkg) {
        $root = ''
        if ($p.CommandLine -match '--x-manifest-root=(\S+)') { $root = " manifest=$($Matches[1])" }
        Write-Warn2 "  pid $($p.ProcessId)$root"
    }
    Write-Warn2 'This run will block on the vcpkg lock until those finish. That wait is silent - it is not a hang.'
}

if ($SkipConfigure) {
    Write-Info 'skipped (-SkipConfigure)'
} else {
    Write-Warn2 'The first configure builds every vcpkg dependency from source - expect SEVERAL HOURS.'
    $cfgSw = [System.Diagnostics.Stopwatch]::StartNew()
    $configureArgs = @('--preset', $LocalPreset)
    # Tests are not built by default; -RunTests implies building them.
    if ($BuildTests -or $RunTests) {
        $configureArgs += '-DBUILD_TESTING=ON'
        Write-Info 'BUILD_TESTING=ON'
    }
    try {
        Invoke-Native -Exe 'cmake' -Arguments $configureArgs -WorkDir $SourceDir `
                      -Because 'cmake configure failed'
    } catch {
        # A source host that answers automated fetches with an anti-bot challenge
        # page returns HTML where a tarball was expected, which surfaces only as an
        # opaque hash mismatch. Name the real problem and the manual way out.
        $manifestLog = Join-Path $BuildDir 'vcpkg-manifest-install.log'
        if (Test-Path $manifestLog) {
            $badHash = Select-String -Path $manifestLog -Pattern 'had an unexpected hash' -EA SilentlyContinue
            if ($badHash) {
                Write-Host ''
                Write-Warn2 'A dependency source download failed its hash check.'
                Write-Warn2 'A common cause is the source host serving an anti-bot challenge page instead of'
                Write-Warn2 'the archive, which cannot be satisfied from the command line. To work around it,'
                Write-Warn2 'download the URL named below in a normal browser, save it into the vcpkg downloads'
                Write-Warn2 "directory under the exact filename vcpkg expects, and re-run this script:"
                $dl = $env:VCPKG_DOWNLOADS
                if (-not $dl) { $dl = Join-Path $VcpkgRoot 'downloads' }
                Write-Warn2 "  downloads dir: $dl"
                Write-Warn2 "  details:       $manifestLog"
            }
        }
        throw
    }
    $cfgSw.Stop()
    Write-Ok ('configured in {0:hh\:mm\:ss}' -f $cfgSw.Elapsed)
}

# ---------------------------------------------------------------------------
Write-Step "Build (--target $Target)"
# ---------------------------------------------------------------------------

if ($SkipBuild) {
    Write-Info 'skipped (-SkipBuild)'
} else {
    $buildArgs = @('--build', $BuildDir)

    # Visual Studio generator presets are multi-config; Ninja presets are not.
    if ($Preset -notlike '*Ninja*') {
        $cfgMap = @{
            'WinRelease'        = 'Release'
            'WinRelWithDebInfo' = 'RelWithDebInfo'
            'WinDebug'          = 'Debug'
        }
        $buildArgs += @('--config', $cfgMap[$Preset])
    }
    if ($Target -ne 'all') { $buildArgs += @('--target', $Target) }
    if ($Jobs -gt 0)       { $buildArgs += @('--parallel', "$Jobs") }

    $bldSw = [System.Diagnostics.Stopwatch]::StartNew()
    Invoke-Native -Exe 'cmake' -Arguments $buildArgs -WorkDir $SourceDir -Because 'cmake --build failed'
    $bldSw.Stop()
    Write-Ok ('built in {0:hh\:mm\:ss}' -f $bldSw.Elapsed)
}

# ---------------------------------------------------------------------------
if ($RunTests) {
    Write-Step 'Tests (ctest)'

    $ctestArgs = @('--test-dir', $BuildDir, '--output-on-failure')
    if ($Preset -notlike '*Ninja*') {
        $cfgMapTest = @{
            'WinRelease'        = 'Release'
            'WinRelWithDebInfo' = 'RelWithDebInfo'
            'WinDebug'          = 'Debug'
        }
        $ctestArgs += @('--build-config', $cfgMapTest[$Preset])
    }
    if ($TestFilter) { $ctestArgs += @('-R', $TestFilter) }
    if ($Jobs -gt 0) { $ctestArgs += @('-j', "$Jobs") }

    # Some tests are known to fail or time out on Windows (and Linux), so a
    # non-zero ctest exit is reported rather than failing the whole build.
    Write-Info "> ctest $($ctestArgs -join ' ')"
    & ctest @ctestArgs
    if ($LASTEXITCODE -eq 0) {
        Write-Ok 'all tests passed'
        Add-Report 'Tests' 'ok'
    } else {
        Write-Warn2 "ctest exited $LASTEXITCODE - some tests fail or time out on Windows, which is expected for now."
        Add-Report 'Tests' 'failures' "ctest exit $LASTEXITCODE"
    }
}

# ---------------------------------------------------------------------------
Write-Step 'Artifacts'
# ---------------------------------------------------------------------------

$launcher  = Join-Path $BuildDir 'run_xstudio.bat'
$installer = Get-ChildItem -Path $BuildDir -Filter 'xSTUDIO-*-win64.exe' -ErrorAction SilentlyContinue |
             Sort-Object LastWriteTime -Descending | Select-Object -First 1

if ($installer) {
    $sizeMb = [math]::Round($installer.Length / 1MB, 1)
    Write-Ok "installer: $($installer.FullName) (${sizeMb}MB)"
    Add-Report 'Installer' 'ok' $installer.FullName
} elseif ($Target -eq 'package') {
    Write-Warn2 'No xSTUDIO-*-win64.exe found in the build folder.'
    Add-Report 'Installer' 'MISSING' $BuildDir
}

if (Test-Path $launcher) {
    Write-Ok "dev launcher: $launcher"
    Add-Report 'Dev launcher' 'ok' $launcher
}

Write-Step 'Summary'
$script:Report | Format-Table -AutoSize
$sw.Stop()
Write-Host ('Total elapsed: {0:hh\:mm\:ss}' -f $sw.Elapsed) -ForegroundColor White
Write-Host "Log: $logFile" -ForegroundColor Gray

if ($Run) {
    if (Test-Path $launcher) {
        Write-Step 'Launching xSTUDIO'
        & $launcher
    } else {
        Write-Warn2 "-Run requested but $launcher does not exist."
    }
}

# Reaching here means the build itself succeeded. Exit 0 explicitly so a failing
# ctest run (reported above as a warning, deliberately not fatal) or any other
# native command does not leak its exit code and make the script look failed.
exit 0

}
catch {
    Write-Host ''
    Write-Fail $_.Exception.Message
    if ($_.ScriptStackTrace) { Write-Host $_.ScriptStackTrace -ForegroundColor DarkGray }
    Write-Host "Log: $logFile" -ForegroundColor Gray
    exit 1
}
finally {
    try { Stop-Transcript | Out-Null } catch { }
}
