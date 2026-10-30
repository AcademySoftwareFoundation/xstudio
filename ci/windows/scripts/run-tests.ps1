# Run C++ unit tests (ctest). Skips Python tests (need a live xstudio session).
#
# Usage: pwsh ci/windows/scripts/run-tests.ps1

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
if (Test-Path Variable:PSNativeCommandUseErrorActionPreference) {
    $PSNativeCommandUseErrorActionPreference = $false
}

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$WindowsDir = Split-Path -Parent $ScriptDir
$RepoRoot = (Resolve-Path (Join-Path $WindowsDir '..\..')).Path
$BuildDir = Join-Path $RepoRoot 'build'

function Write-Log([string]$Message) {
    Write-Host ""
    Write-Host "==> $Message" -ForegroundColor Cyan
}

if (-not (Test-Path -LiteralPath $BuildDir)) {
    throw "ERROR: build tree not found at $BuildDir`n  Run ci/windows/scripts/build-xstudio.ps1 first."
}

$Jobs = if ($env:JOBS) { $env:JOBS } else { '1' }

Write-Log "Running ctest in $BuildDir"
& ctest --test-dir $BuildDir --output-on-failure -j $Jobs -E 'python_tests'
if ($LASTEXITCODE -ne 0) {
    throw "ctest failed (exit $LASTEXITCODE)"
}

Write-Log 'Windows tests OK'
