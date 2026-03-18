param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",
    [switch]$Clean
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Resolve-Path (Join-Path $scriptDir "..")
Set-Location $repoRoot

$preset = "windows-debug"
$buildDir = Join-Path $repoRoot "build-win"

if ($Clean -and (Test-Path $buildDir)) {
    Write-Host "Removing existing build directory: $buildDir"
    Remove-Item -Recurse -Force $buildDir
}

Write-Host "Configuring ($preset)..."
cmake --preset $preset

Write-Host "Building ($Configuration)..."
cmake --build --preset $preset --config $Configuration

Write-Host "Running tests ($Configuration)..."
ctest --preset $preset -C $Configuration

Write-Host "Windows verification succeeded."
