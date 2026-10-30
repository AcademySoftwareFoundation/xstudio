# Download a pinned sccache release binary into DEST_DIR (default: .ci-cache/sccache-bin).
#
# Usage: ci/windows/scripts/install-sccache.ps1
#
# Env:
#   SCCACHE_VERSION=v0.19.0
#   DEST_DIR / SCCACHE_BIN_DIR=.ci-cache/sccache-bin

$ErrorActionPreference = 'Stop'

$version = if ($env:SCCACHE_VERSION) { $env:SCCACHE_VERSION } else { 'v0.19.0' }
$dest = if ($env:DEST_DIR) {
    $env:DEST_DIR
} elseif ($env:SCCACHE_BIN_DIR) {
    $env:SCCACHE_BIN_DIR
} else {
    '.ci-cache/sccache-bin'
}

New-Item -ItemType Directory -Force -Path $dest | Out-Null

$asset = "sccache-$version-x86_64-pc-windows-msvc.zip"
$url = "https://github.com/mozilla/sccache/releases/download/$version/$asset"
$zip = Join-Path $env:RUNNER_TEMP $asset
$extract = Join-Path $env:RUNNER_TEMP "sccache-$version"

Write-Host "==> Downloading $url"
Invoke-WebRequest -Uri $url -OutFile $zip
if (Test-Path -LiteralPath $extract) {
    Remove-Item -Recurse -Force -LiteralPath $extract
}
Expand-Archive -Path $zip -DestinationPath $extract -Force

$exe = Get-ChildItem -Path $extract -Recurse -Filter 'sccache.exe' | Select-Object -First 1
if (-not $exe) {
    throw "sccache.exe not found in $asset"
}
Copy-Item -Force $exe.FullName (Join-Path $dest 'sccache.exe')
& (Join-Path $dest 'sccache.exe') --version
if ($LASTEXITCODE -ne 0) {
    throw "sccache --version failed ($LASTEXITCODE)"
}
