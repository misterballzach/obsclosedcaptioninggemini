# OBS Gemini Captions Plugin

A C++ plugin for OBS Studio that provides closed captioning using the Google Gemini API (1.5 Flash). It captures audio from a selected source, transcribes it, and outputs captions to the stream (CEA-608) and a local dock.

Also includes a Twitch Bot that responds to `!gemini` in chat.

## ⚠️ CRITICAL BUILD INSTRUCTIONS ⚠️

**You MUST follow these steps to build, as OBS 32.0.4+ does not have a standard SDK download.**

### Prerequisites

1.  **Visual Studio 2022** (with C++ Desktop Development workload).
2.  **Qt 6** (ensure `Qt6Config.cmake` is in your PATH or CMake can find it).
    *   *Note: The easy build script will ask for the path if needed.*
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

**Option A: The Easy Way (Recommended)**
Double-click `easy-build.bat`.
*   It will ask for your Qt path if not found.
*   It will configure and build the Release version.
*   It will open the output folder.

**Option B: Manual Build**
1.  `mkdir build` -> `cd build`
2.  `cmake .. -A x64` (Add `-DQt6_DIR="C:\Path\To\Qt\..."` if needed)
3.  `cmake --build . --config Release`

### Step 3: Install/Run

1.  **Locate the Plugin:**
    *   Go to `build/Release/`.
    *   Find `obs-gemini-captions.dll`. (Do NOT copy `.exp`, `.lib`, or `.pdb` files).

2.  **Copy the DLL:**
    Copy `obs-gemini-captions.dll` to your OBS plugins folder:
    `C:\Program Files\obs-studio\obs-plugins\64bit\`

3.  **Copy the Data:**
    Copy the `data` folder from the source repository to the OBS data folder:
    From: `obs-gemini-captions/data`
    To:   `C:\Program Files\obs-studio\data\obs-plugins\obs-gemini-captions`
    *(The final path should contain `locale/en-US.ini`)*

## Features

*   **Audio Capture:** Resamples source audio to 16kHz Mono using global OBS audio context.
*   **Buffering:** Buffers 5 seconds of audio before sending to avoid rate limits.
*   **Async Processing:** Uses background threads for network API calls.
*   **Twitch Bot:** `!gemini <prompt>` in chat gets an AI response.
*   **Dock:** View past captions in a custom dockable widget.

## Configuration

Go to **Tools -> Gemini Captions** to configure your API Key and Twitch credentials.

## Troubleshooting

### "Could not find a package configuration file provided by Qt6"
CMake cannot find your Qt installation.
1.  Run `easy-build.bat`.
2.  When prompted, paste the full path to your Qt CMake folder (e.g., `C:\Qt\6.6.2\msvc2019_64\lib\cmake\Qt6`).

### "Plugin Load Error"
If OBS shows an error saying the plugin failed to load:
1.  **Check Build Mode:** You likely built in **Debug** mode. OBS requires **Release** mode plugins. Run `easy-build.bat`.
2.  **Check Path:** Ensure `obs-gemini-captions.dll` is in `obs-plugins/64bit`.

### "Release folder missing?"
If you only see a `Debug` folder inside `build`, you skipped the Release build step. Run `easy-build.bat`.
