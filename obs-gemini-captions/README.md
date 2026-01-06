# OBS Gemini Captions Plugin

A C++ plugin for OBS Studio that provides closed captioning using the Google Gemini API (1.5 Flash). It captures audio from a selected source, transcribes it, and outputs captions to the stream (CEA-608) and a local dock.

Also includes a Twitch Bot that responds to `!gemini` in chat.

## ⚠️ CRITICAL BUILD INSTRUCTIONS ⚠️

**You MUST follow these steps to build, as OBS 32.0.4+ does not have a standard SDK download.**

### Prerequisites

1.  **Visual Studio 2022** (with C++ Desktop Development workload).
2.  **Qt 6** (ensure `Qt6Config.cmake` is in your PATH or CMake can find it).
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

### Step 2: Build with CMake

1.  Create a build directory:
    ```cmd
    mkdir build
    cd build
    ```
2.  Configure:
    ```cmd
    cmake .. -G "Visual Studio 17 2022" -A x64
    ```
3.  Build:
    ```cmd
    cmake --build . --config Release
    ```

### Step 3: Install/Run

Copy the built `obs-gemini-captions.dll` to your OBS plugins folder (usually `C:\Program Files\obs-studio\obs-plugins\64bit`), and copy the `data` folder to `C:\Program Files\obs-studio\data\obs-plugins\obs-gemini-captions`.

## Features

*   **Audio Capture:** Resamples source audio to 16kHz Mono.
*   **Buffering:** Buffers 5 seconds of audio before sending to avoid rate limits.
*   **Async Processing:** Uses background threads for network API calls.
*   **Twitch Bot:** `!gemini <prompt>` in chat gets an AI response.
*   **Dock:** View past captions in a custom dockable widget.

## Configuration

Go to **Tools -> Gemini Captions** to configure your API Key and Twitch credentials.
