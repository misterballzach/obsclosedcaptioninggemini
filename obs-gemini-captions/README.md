# OBS Gemini Captions Plugin

A C++ plugin for OBS Studio that provides closed captioning using the Google Gemini API (1.5 Flash). It captures audio from a selected source, transcribes it, and outputs captions to the stream (CEA-608) and a local dock.

Also includes a Twitch Bot that responds to `!gemini` in chat.

## ⚠️ CRITICAL COMPATIBILITY NOTE

**Qt Version Mismatch:**
This plugin links against Qt 6. OBS Studio 32.0.4 uses **Qt 6.8**.
You **MUST** build this plugin using a Qt version compatible with 6.8 (e.g., 6.8.x, 6.7.x, or sometimes older 6.x).
If you build with a *newer* Qt (e.g., bleeding edge 6.10), the plugin will **FAIL TO LOAD** in OBS.

## ⚠️ CRITICAL BUILD INSTRUCTIONS ⚠️

**You MUST follow these steps to build, as OBS 32.0.4+ does not have a standard SDK download.**

### Prerequisites

1.  **Visual Studio 2022** (with C++ Desktop Development workload).
2.  **Qt 6.8** (Recommended).
3.  **CMake 3.16+**.

### Step 1: Generate the SDK

1.  Open **"x64 Native Tools Command Prompt for VS 2022"**.
2.  Navigate to this folder:
    ```cmd
    cd obs-gemini-captions
    ```
3.  Run the setup script:
    ```powershell
    powershell -ExecutionPolicy Bypass -File setup-sdk.ps1
    ```
    *This will download OBS 32.0.4 binaries and source code to `C:\obs-sdk`, and generate the required `.lib` files.*

    **NOTE:** If you see CMake errors about "non-existent path" later, RUN THIS SCRIPT AGAIN.

### Step 2: Build the Plugin

**The Easy Way (Recommended)**
Double-click `easy-build.bat`.
*   It will ask for your Qt path if not found.
*   It will configure and build the **Release** version automatically.

### Step 3: Install

**The Easy Way**
Right-click `install-plugin.bat` and select **Run as Administrator**.
*   It will copy the DLL and Data files to your OBS installation.

**Manual Install**
1.  Copy `build/Release/obs-gemini-captions.dll` to `C:\Program Files\obs-studio\obs-plugins\64bit\`.
2.  Copy `data` folder to `C:\Program Files\obs-studio\data\obs-plugins\obs-gemini-captions`.

## Features

*   **Audio Capture:** Resamples source audio to 16kHz Mono using global OBS audio context.
*   **Buffering:** Buffers 5 seconds of audio before sending to avoid rate limits.
*   **Async Processing:** Uses background threads for network API calls.
*   **Twitch Bot:** `!gemini <prompt>` in chat gets an AI response.
*   **Dock:** View past captions in a custom dockable widget.

## Configuration

Go to **Tools -> Gemini Captions** to configure your API Key and Twitch credentials.

## Troubleshooting

### "Plugin Load Error"
If OBS shows an error saying the plugin failed to load:
1.  **Check Build Mode:** You likely built in **Debug** mode. OBS requires **Release** mode plugins. Run `easy-build.bat`.
2.  **Check Qt Version:** If you built against Qt 6.10+ but are running OBS with Qt 6.8, it will fail. Install Qt 6.8 and rebuild (delete `build` folder first).
3.  **Check Path:** Ensure `obs-gemini-captions.dll` is in `obs-plugins/64bit`.

### "Release folder missing?"
If you only see a `Debug` folder inside `build`, you skipped the Release build step. Run `easy-build.bat`.
