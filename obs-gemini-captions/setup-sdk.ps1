<#
.SYNOPSIS
    Sets up the OBS Studio SDK for version 32.0.4 by downloading the binary release and source code,
    and generating the necessary .lib files using MSVC tools.

.DESCRIPTION
    OBS Studio 32.0.4+ does not provide a pre-compiled SDK zip. This script creates one.
    It performs the following steps:
    1. Checks for 'dumpbin' and 'lib' (MSVC tools).
    2. Downloads OBS Studio binaries and source code.
    3. Extracts them.
    4. Generates 'obs.lib' and 'obs-frontend-api.lib' from the DLLs.
    5. Organizes files into a 'obs-sdk' folder structure compatible with CMake.

.NOTES
    You MUST run this from the "x64 Native Tools Command Prompt for VS 2022" (or similar)
    so that 'dumpbin' and 'lib' are in your PATH.
#>

$Version = "32.0.4"
$WorkDir = Join-Path $PSScriptRoot "obs-sdk-setup"
$SdkDir = "C:\obs-sdk"

# check permissions
if (-not (Test-Path $SdkDir)) {
    try {
        New-Item -ItemType Directory -Force -Path $SdkDir | Out-Null
    } catch {
        Write-Error "Cannot create $SdkDir. Run as Administrator or choose a different folder."
        exit 1
    }
}

# Check for tools
if (-not (Get-Command "dumpbin" -ErrorAction SilentlyContinue) -or -not (Get-Command "lib" -ErrorAction SilentlyContinue)) {
    Write-Error "MSVC tools 'dumpbin' or 'lib' not found!"
    Write-Warning "Please run this script from the 'x64 Native Tools Command Prompt for VS 2022'."
    exit 1
}

Write-Host "Setting up OBS SDK $Version in $SdkDir..." -ForegroundColor Cyan

# Create work dir
New-Item -ItemType Directory -Force -Path $WorkDir | Out-Null

# URLs
$RuntimeZip = "OBS-Studio-$Version-Windows-x64.zip"
$RuntimeUrl = "https://github.com/obsproject/obs-studio/releases/download/$Version/$RuntimeZip"
$SourceZip = "obs-studio-$Version.zip"
$SourceUrl = "https://github.com/obsproject/obs-studio/archive/refs/tags/$Version.zip"

# Download
$WebClient = New-Object System.Net.WebClient

if (-not (Test-Path (Join-Path $WorkDir $RuntimeZip))) {
    Write-Host "Downloading Binaries..."
    $WebClient.DownloadFile($RuntimeUrl, (Join-Path $WorkDir $RuntimeZip))
}

if (-not (Test-Path (Join-Path $WorkDir $SourceZip))) {
    Write-Host "Downloading Source Code..."
    $WebClient.DownloadFile($SourceUrl, (Join-Path $WorkDir $SourceZip))
}

# Extract
Write-Host "Extracting..."
Expand-Archive -Path (Join-Path $WorkDir $RuntimeZip) -DestinationPath (Join-Path $WorkDir "runtime") -Force
Expand-Archive -Path (Join-Path $WorkDir $SourceZip) -DestinationPath (Join-Path $WorkDir "source") -Force

# Locate extracted folders
$BinDir = Join-Path $WorkDir "runtime\bin\64bit"
# Source usually extracts to "obs-studio-32.0.4"
$SrcRoot = (Get-ChildItem -Path (Join-Path $WorkDir "source") -Directory).FullName

# Function to generate .lib
function Generate-Lib {
    param(
        [string]$DllPath,
        [string]$OutLib
    )
    $DefFile = $OutLib.Replace(".lib", ".def")
    $DllName = [System.IO.Path]::GetFileName($DllPath)

    Write-Host "Generating lib for $DllName..."

    $Exports = cmd /c "dumpbin /exports `"$DllPath`""

    $Lines = @("LIBRARY $DllName", "EXPORTS")
    foreach ($Line in $Exports) {
        # Format: ordinal hint RVA name
        # Example: 1    0 00012345 obs_function
        if ($Line -match "^\s+\d+\s+[0-9A-F]+\s+[0-9A-F]+\s+(?<name>\S+)") {
            $Lines += $Matches.name
        }
    }

    Set-Content -Path $DefFile -Value $Lines

    cmd /c "lib /def:`"$DefFile`" /out:`"$OutLib`" /machine:x64" | Out-Null
}

# 1. Setup Include Dirs
Write-Host "Copying headers..."
$IncludeDir = Join-Path $SdkDir "include"
New-Item -ItemType Directory -Force -Path $IncludeDir | Out-Null

# LibObs Headers
Copy-Item -Recurse -Force (Join-Path $SrcRoot "libobs") (Join-Path $IncludeDir "libobs")
# Frontend API Headers
$FrontendInc = Join-Path $IncludeDir "obs-frontend-api"
New-Item -ItemType Directory -Force -Path $FrontendInc | Out-Null
Copy-Item -Force (Join-Path $SrcRoot "UI\obs-frontend-api\*.h") $FrontendInc

# 3. Generate obsconfig.h (Required for compilation)
Write-Host "Generating obsconfig.h..."
$ObsConfigContent = @"
#pragma once
#define OBS_VERSION "$Version"
#define OBS_DATA_PATH "../../data"
#define OBS_INSTALL_PREFIX ""
#define OBS_PLUGIN_DESTINATION "obs-plugins"
#define OBS_RELATIVE_PREFIX "../../"
#define OBS_QT_VERSION 6
#define ON 1
#define OFF 0
"@
Set-Content -Path (Join-Path $IncludeDir "libobs\obsconfig.h") -Value $ObsConfigContent

# 2. Setup Bin Dirs & Generate Libs
Write-Host "Generating libraries..."
$BinSdkDir = Join-Path $SdkDir "bin\64bit"
New-Item -ItemType Directory -Force -Path $BinSdkDir | Out-Null

# Copy DLLs
Copy-Item -Force (Join-Path $BinDir "obs.dll") $BinSdkDir
Copy-Item -Force (Join-Path $BinDir "obs-frontend-api.dll") $BinSdkDir

# Generate Libs
Generate-Lib (Join-Path $BinSdkDir "obs.dll") (Join-Path $BinSdkDir "obs.lib")
Generate-Lib (Join-Path $BinSdkDir "obs-frontend-api.dll") (Join-Path $BinSdkDir "obs-frontend-api.lib")

# Cleanup
Write-Host "Cleaning up temp files..."
Remove-Item -Recurse -Force $WorkDir

Write-Host "Success! OBS SDK installed to $SdkDir" -ForegroundColor Green
Write-Host "You can now run CMake."
