# OBS Gemini Captions Plugin

This plugin for OBS Studio provides closed captioning using the Google Gemini API (gemini-1.5-flash). It captures audio from a selected source, transcribes it, and outputs text to both an OBS Text Source (Open Captions) and the streaming output (Closed Captions/CEA-608).

## Prerequisites

To build this plugin, you need:

*   **CMake** (3.16 or newer)
*   **C++ Compiler** (supporting C++17)
    *   Windows: Visual Studio 2022 recommended
    *   macOS: Xcode
    *   Linux: GCC or Clang
*   **Qt 6** (Core and Widgets components)
*   **libcurl**
*   **OBS Studio Development Files** (libobs)
    *   You can build OBS Studio from source, or use pre-compiled libraries if available for your platform.

## Build Instructions

### Windows

1.  **Get OBS Studio Source:** Clone the OBS Studio repository and build it, or download a pre-built SDK if available.
2.  **Open CMake GUI:**
    *   Set **Source code** to the `obs-gemini-captions` folder.
    *   Set **Build the binaries** to a new `build` folder inside it.
3.  **Configure:**
    *   Click **Configure**.
    *   Specify the path to your Qt 6 installation if prompted (e.g., `Qt6_DIR`).
    *   Specify the path to `libobs` if not found (set `LIBOBS_INCLUDE_DIRS` and `LIBOBS_LIBRARIES`).
4.  **Generate:** Click **Generate**, then **Open Project** (opens Visual Studio).
5.  **Build:** Build the solution in **Release** mode.

### macOS

```bash
mkdir build && cd build
cmake -DQT_DIR=/path/to/Qt/6.x.x/macos/lib/cmake/Qt6 ..
make
```

### Linux

```bash
mkdir build && cd build
cmake ..
make
```

## Installation

After building, you need to copy the plugin files to your OBS Studio installation directory.

### Windows

1.  Copy `obs-gemini-captions.dll` from your build output (e.g., `build/Release`) to:
    `C:\Program Files\obs-studio\obs-plugins\64bit\`
2.  Copy the `data` folder content:
    *   Create folder: `C:\Program Files\obs-studio\data\obs-plugins\obs-gemini-captions`
    *   Copy the `locale` folder (from `obs-gemini-captions/data/`) into that new directory.

### macOS

1.  Copy `obs-gemini-captions.so` (or `.dylib`) to:
    `/Library/Application Support/obs-studio/plugins/`
    (Or right-click OBS.app -> Show Package Contents -> PlugIns)
2.  Copy the data files to the corresponding data directory.

### Linux

1.  Copy `obs-gemini-captions.so` to:
    `~/.config/obs-studio/plugins/obs-gemini-captions/bin/64bit/`
2.  Copy the data files to:
    `~/.config/obs-studio/plugins/obs-gemini-captions/data/`

## Usage

1.  Open OBS Studio.
2.  Go to **Tools** -> **Gemini Captions**.
3.  Enter your **Gemini API Key**.
4.  Select the **Audio Source** you want to caption (e.g., Mic/Aux).
5.  (Optional) Select a **Text Source** to display captions on screen (Open Captions).
6.  Click **Start Captioning**.

## Notes

*   This plugin uses the Gemini 1.5 Flash model via REST API.
*   It buffers audio in 3-second chunks.
*   Requires an active internet connection.
