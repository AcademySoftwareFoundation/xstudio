# Install Windows build deps: Qt SDK via aqt, vcpkg + manifest ports.
# No xSTUDIO configure/build here — see build-xstudio.ps1.
#
# Usage: pwsh ci/windows/scripts/build-deps.ps1
#
# Overridable env:
#   JOBS=1
#   QT_ROOT=C:\Qt
#   VCPKG_ROOT=<sibling-of-repo>\vcpkg

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
if (-not $env:AQTINSTALL_VERSION) { throw 'AQTINSTALL_VERSION missing from ci/windows/versions.env' }

$VcpkgBaseline = (
    Get-Content -Raw -LiteralPath (Join-Path $RepoRoot 'vcpkg.json') |
    ConvertFrom-Json
).'builtin-baseline'
if (-not $VcpkgBaseline) { throw 'could not read builtin-baseline from vcpkg.json' }

$Jobs = if ($env:JOBS) { $env:JOBS } else { '1' }
$env:JOBS = $Jobs

$QtRoot = if ($env:QT_ROOT) { $env:QT_ROOT } else { 'C:\Qt' }
$env:QT_ROOT = $QtRoot

$VcpkgRoot = if ($env:VCPKG_ROOT) {
    $env:VCPKG_ROOT
} else {
    Join-Path (Split-Path -Parent $RepoRoot) 'vcpkg'
}
$env:VCPKG_ROOT = $VcpkgRoot

$QtPrefix = Join-Path $QtRoot "$($env:QT_VERSION)\msvc2019_64"
$Qt6Dir = Join-Path $QtPrefix 'lib\cmake\Qt6'
$VcpkgExe = Join-Path $VcpkgRoot 'vcpkg.exe'

Write-Log "Repo:       $RepoRoot"
Write-Log "Qt root:    $QtRoot (version $($env:QT_VERSION))"
Write-Log "vcpkg root: $VcpkgRoot (baseline $VcpkgBaseline)"
Write-Log "Jobs:       $Jobs"

# --- Qt SDK -------------------------------------------------------------------
if (Test-Path -LiteralPath $Qt6Dir) {
    Write-Log "Qt already present at $QtPrefix (skipping aqtinstall)"
} else {
    $AqtVenv = Join-Path $RepoRoot '.ci-cache\aqt-venv'
    Write-Log "Installing aqtinstall==$($env:AQTINSTALL_VERSION) into $AqtVenv"
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $AqtVenv) | Out-Null
    Invoke-Checked python @('-m', 'venv', $AqtVenv)
    $Pip = Join-Path $AqtVenv 'Scripts\pip.exe'
    $Python = Join-Path $AqtVenv 'Scripts\python.exe'
    Invoke-Checked $Pip @('install', '--quiet', "aqtinstall==$($env:AQTINSTALL_VERSION)")

    Write-Log "Installing Qt $($env:QT_VERSION) (win64_msvc2019_64 + qtimageformats + qtpdf) into $QtRoot"
    Invoke-Checked $Python @(
        '-m', 'aqt', 'install-qt', 'windows', 'desktop', $env:QT_VERSION, 'win64_msvc2019_64',
        '--modules', 'qtimageformats', 'qtpdf',
        '--outputdir', $QtRoot
    )
}

if (-not (Test-Path -LiteralPath $Qt6Dir)) {
    throw "ERROR: Qt6 cmake package not found at $Qt6Dir"
}
Write-Log "Using Qt6_DIR=$Qt6Dir"

# --- vcpkg bootstrap (skipped when CI already bootstrapped this baseline) ------
$NeedBootstrap = $true
if (Test-Path -LiteralPath $VcpkgExe) {
    Push-Location $VcpkgRoot
    try {
        $Head = (git rev-parse HEAD 2>$null)
        if ($Head -eq $VcpkgBaseline) { $NeedBootstrap = $false }
    } finally {
        Pop-Location
    }
}

if ($NeedBootstrap) {
    # Full clone: versioned port git-trees must be in the local object DB
    # (blobless/shallow clones force promisor fetches during install).
    Write-Log "Fetching vcpkg $VcpkgBaseline"
    if (-not (Test-Path -LiteralPath (Join-Path $VcpkgRoot '.git'))) {
        Invoke-Checked git @('clone', 'https://github.com/microsoft/vcpkg.git', $VcpkgRoot)
    } else {
        Invoke-Checked git @('-C', $VcpkgRoot, 'fetch', 'origin')
    }
    Invoke-Checked git @('-C', $VcpkgRoot, 'checkout', '--force', $VcpkgBaseline)
    Write-Log 'Bootstrapping vcpkg'
    $Bootstrap = Join-Path $VcpkgRoot 'bootstrap-vcpkg.bat'
    Invoke-Checked cmd.exe @('/c', "`"$Bootstrap`" -disableMetrics")
} else {
    Write-Log "vcpkg already at $VcpkgBaseline (skip bootstrap)"
}

# --- vcpkg manifest ports -----------------------------------------------------
Write-Log 'Installing vcpkg manifest dependencies (triplet x64-windows)'
$OverlayPorts = Join-Path $RepoRoot 'cmake\vcpkg_overlay_ports'
Push-Location $RepoRoot
try {
    Invoke-Checked $VcpkgExe @(
        'install',
        '--triplet', 'x64-windows',
        "--overlay-ports=$OverlayPorts"
    )
} finally {
    Pop-Location
}

Write-Log 'Windows deps OK'
