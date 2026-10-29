# Configure and build xSTUDIO on Windows (x64 MSVC / Ninja).
# Expects deps already installed (ci/windows/scripts/build-deps.ps1).
#
# Usage: pwsh ci/windows/scripts/build-xstudio.ps1
#
# Overridable env:
#   JOBS=1
#   QT_ROOT=C:\Qt
#   VCPKG_ROOT=<sibling-of-repo>\vcpkg
#   BUILD_TESTING=OFF

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if (Test-Path Variable:PSNativeCommandUseErrorActionPreference) {
    $PSNativeCommandUseErrorActionPreference = $false
}

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$WindowsDir = Split-Path -Parent $ScriptDir
$RepoRoot = (Resolve-Path (Join-Path $WindowsDir '..\..')).Path

function Write-Log([string]$Message) {
    Write-Host ""
    Write-Host "==> $Message" -ForegroundColor Cyan
}

function Import-VersionsEnv([string]$Path) {
    Get-Content -LiteralPath $Path | ForEach-Object {
        $line = $_.Trim()
        if (-not $line -or $line.StartsWith('#')) { return }
        $eq = $line.IndexOf('=')
        if ($eq -lt 1) { return }
        $name = $line.Substring(0, $eq).Trim()
        $value = $line.Substring($eq + 1).Trim()
        Set-Item -Path "Env:$name" -Value $value
    }
}

function Invoke-Checked([string]$FilePath, [string[]]$ArgumentList = @()) {
    & $FilePath @ArgumentList
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed (exit $LASTEXITCODE): $FilePath $($ArgumentList -join ' ')"
    }
}

Import-VersionsEnv (Join-Path $WindowsDir 'versions.env')
if (-not $env:QT_VERSION) { throw 'QT_VERSION missing from ci/windows/versions.env' }

$Jobs = if ($env:JOBS) { $env:JOBS } else { '1' }
$env:JOBS = $Jobs
$BuildTesting = if ($env:BUILD_TESTING) { $env:BUILD_TESTING } else { 'OFF' }

$QtRoot = if ($env:QT_ROOT) { $env:QT_ROOT } else { 'C:\Qt' }
$VcpkgRoot = if ($env:VCPKG_ROOT) {
    $env:VCPKG_ROOT
} else {
    Join-Path (Split-Path -Parent $RepoRoot) 'vcpkg'
}

$Qt6Dir = Join-Path $QtRoot "$($env:QT_VERSION)\msvc2019_64\lib\cmake\Qt6"
$VcpkgExe = Join-Path $VcpkgRoot 'vcpkg.exe'
$BuildDir = Join-Path $RepoRoot 'build'

if (-not (Test-Path -LiteralPath $Qt6Dir)) {
    throw "ERROR: Qt6 cmake package not found at $Qt6Dir`n  Run ci/windows/scripts/build-deps.ps1 first."
}
if (-not (Test-Path -LiteralPath $VcpkgExe)) {
    throw "ERROR: vcpkg not found at $VcpkgRoot`n  Run ci/windows/scripts/build-deps.ps1 first."
}

Write-Log 'Configuring with preset WinNinjaRelease'
Invoke-Checked cmake @(
    '-S', $RepoRoot,
    '-B', $BuildDir,
    '--preset', 'WinNinjaRelease',
    "-DQt6_DIR=$Qt6Dir",
    "-DBUILD_TESTING=$BuildTesting"
)

Write-Log 'Building xSTUDIO'
Invoke-Checked cmake @('--build', $BuildDir, '-j', $Jobs)

Write-Log 'Windows build OK'
