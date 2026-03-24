param(
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Debug",
    [switch]$Clean,
    [ValidateSet("auto", "vs2022", "ninja")]
    [string]$Toolchain = "auto"
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

function Invoke-External {
    param(
        [Parameter(Mandatory = $true)]
        [string]$FilePath,
        [Parameter(ValueFromRemainingArguments = $true)]
        [string[]]$Arguments
    )

    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed with exit code {$LASTEXITCODE}: $FilePath $($Arguments -join ' ')"
    }
}

function Test-VisualStudio2022Installed {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) {
        return $false
    }

    $installationPath = & $vswhere -latest -version "[17.0,18.0)" -products * -property installationPath
    return -not [string]::IsNullOrWhiteSpace($installationPath)
}

function Ensure-TestsExist {
    param(
        [Parameter(Mandatory = $true)]
        [string]$BuildDir,
        [switch]$MultiConfig,
        [Parameter(Mandatory = $true)]
        [string]$Configuration
    )

    $args = @("--test-dir", $BuildDir, "-N")
    if ($MultiConfig) {
        $args += @("-C", $Configuration)
    }

    $listOutput = & ctest @args
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to enumerate tests in '$BuildDir'."
    }

    $outputText = ($listOutput | Out-String)
    if ($outputText -match "Total Tests:\s+0" -or $outputText -match "No tests were found!!!") {
        throw "No tests were discovered in '$BuildDir'."
    }
}

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Resolve-Path (Join-Path $scriptDir "..")
Set-Location $repoRoot

$useVs = $false
switch ($Toolchain) {
    "vs2022" {
        if (-not (Test-VisualStudio2022Installed)) {
            throw "Visual Studio 2022 was requested but is not installed."
        }
        $useVs = $true
    }
    "ninja" {
        if (-not (Get-Command ninja -ErrorAction SilentlyContinue)) {
            throw "Ninja was requested but is not available in PATH."
        }
        $useVs = $false
    }
    default {
        $useVs = Test-VisualStudio2022Installed
        if (-not $useVs -and -not (Get-Command ninja -ErrorAction SilentlyContinue)) {
            throw "No supported toolchain found. Install Visual Studio 2022 Build Tools or Ninja."
        }
    }
}

if ($useVs) {
    $buildDir = Join-Path $repoRoot "build-win-vs"
    Write-Host "Using Visual Studio 2022 generator."
    if ($Clean -and (Test-Path $buildDir)) {
        Write-Host "Removing existing build directory: $buildDir"
        Remove-Item -Recurse -Force $buildDir
    }

    Write-Host "Configuring (Visual Studio 2022, x64)..."
    Invoke-External cmake -S . -B $buildDir -G "Visual Studio 17 2022" -A x64

    Write-Host "Building ($Configuration)..."
    Invoke-External cmake --build $buildDir --config $Configuration

    Write-Host "Checking test discovery..."
    Ensure-TestsExist -BuildDir $buildDir -MultiConfig -Configuration $Configuration

    Write-Host "Running tests ($Configuration)..."
    Invoke-External ctest --test-dir $buildDir -C $Configuration --output-on-failure
} else {
    $buildDir = Join-Path $repoRoot "build-win-ninja"
    Write-Host "Using Ninja generator."
    if ($Clean -and (Test-Path $buildDir)) {
        Write-Host "Removing existing build directory: $buildDir"
        Remove-Item -Recurse -Force $buildDir
    }

    Write-Host "Configuring (Ninja, $Configuration)..."
    Invoke-External cmake -S . -B $buildDir -G Ninja -DCMAKE_BUILD_TYPE=$Configuration

    Write-Host "Building ($Configuration)..."
    Invoke-External cmake --build $buildDir

    Write-Host "Checking test discovery..."
    Ensure-TestsExist -BuildDir $buildDir -Configuration $Configuration

    Write-Host "Running tests ($Configuration)..."
    Invoke-External ctest --test-dir $buildDir --output-on-failure
}

Write-Host "Windows verification succeeded."
