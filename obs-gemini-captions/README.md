# OBS Gemini Captions Plugin (The "How-To" Guide)

So, you want closed captions in OBS using Google's fancy Gemini AI, and you also want a Twitch bot that answers questions? You've come to the right place.

**Warning:** This is "Source Code". It's like a pile of car parts. You have to assemble the car (compile it) before you can drive it. If you have a friend who is a programmer, buy them a pizza and ask them to do this for you. If not, follow these steps exactly.

---

## Part 1: Download the Tools (Windows)

We are assuming you are on Windows because that's what most streamers use.

1.  **Visual Studio 2022 Community** (Free)
    *   Download: [https://visualstudio.microsoft.com/vs/community/](https://visualstudio.microsoft.com/vs/community/)
    *   **Crucial Step:** When installing, check the box that says **"Desktop development with C++"**. If you miss this, nothing will work.

2.  **CMake**
    *   Download: [https://cmake.org/download/](https://cmake.org/download/)
    *   Get the `Windows x64 Installer`.
    *   Install it. When asked, select **"Add CMake to the system PATH for all users"**.

3.  **Qt 6**
    *   Download the "Online Installer": [https://www.qt.io/download-qt-installer](https://www.qt.io/download-qt-installer)
    *   Run it, log in.
    *   In the "Select Components" screen:
        *   Expand **Qt 6.x.x** (pick the latest 6.x version, e.g., 6.10 or 6.8).
        *   **RECOMMENDED:** Check **MSVC 2022 64-bit**.
        *   Check **Qt Network** (often included in base, but double check).
    *   Remember where you installed it! Usually `C:\Qt`.

---

## Part 2: Generate the OBS SDK (IMPORTANT!)

OBS 32.0.4+ no longer provides a pre-compiled SDK. You must generate one using the included script.

1.  **Open the "x64 Native Tools Command Prompt for VS 2022"**.
    *   Press Windows Key, type "x64 Native", and you should see it.
    *   **Right-click and Run as Administrator** (needed to write to C:\obs-sdk).

2.  **Go to the plugin folder**.
    *   Type: `cd C:\path\to\obs-gemini-captions` (change this to where you downloaded this code).

3.  **Run the Setup Script**.
    *   Type: `powershell -ExecutionPolicy Bypass -File setup-sdk.ps1`
    *   It will download OBS 32.0.4, extract it, and generate the required `.lib` files.
    *   Wait for it to say **"Success! OBS SDK installed to C:\obs-sdk"**.
    *   **NOTE:** If you see `Cannot open include file: 'obsconfig.h'` later, you MUST re-run this script (we updated it!).

---

## Part 3: Building the Plugin

1.  **Open CMake (cmake-gui)**.
2.  **Where is source code:** The folder with `CMakeLists.txt`.
3.  **Where to build:** Create a `build` folder.
4.  **Configure:**
    *   Select **Visual Studio 17 2022**.
    *   Platform: **x64**.
5.  **Fix Errors (First Run):**
    *   Set `LIBOBS_INCLUDE_DIR` to `C:\obs-sdk\include\libobs`.
    *   Set `LIBOBS_LIB` to `C:\obs-sdk\bin\64bit\obs.lib`.
    *   Set `Qt6_DIR` to your Qt MSVC folder (e.g., `C:\Qt\6.10.1\msvc2022_64\lib\cmake\Qt6`).
6.  **Click Configure Again.**
    *   **"Configuring done"** is what you want to see.
    *   *Note:* Ignore red text about `pthread` or `Vulkan` if "Configuring done" appears.
7.  **Generate** -> **Open Project** -> Build in Visual Studio.

---

## Part 4: Putting it in OBS

1.  **Find the `.dll` file**
    *   Copy `obs-gemini-captions.dll` from your build folder.
    *   (Usually in `build/Release` or just `build`).
2.  **Go to OBS Install Folder**
    *   `C:\Program Files\obs-studio\obs-plugins\64bit`.
    *   Paste the `.dll`.
3.  **Install Data**
    *   Copy the `data` folder from source to `C:\Program Files\obs-studio\data\obs-plugins`.
    *   Rename folder to `obs-gemini-captions`.

---

## Part 5: How to Use It

1.  **Get a Gemini API Key** from [Google AI Studio](https://aistudio.google.com/).
2.  **Start OBS** -> **Tools** -> **Gemini Captions**.
3.  Enter API Key and select Audio Source.
4.  Click **Start Captioning**.

---

## Troubleshooting

*   **"Cannot open include file: 'obsconfig.h'"**:
    *   This file is missing because the previous version of the setup script didn't generate it.
    *   **SOLUTION:** Run `setup-sdk.ps1` again! It will fix it.

*   **"setup-sdk.ps1 failed!"**:
    *   Did you run as Administrator?
    *   Did you use the "x64 Native Tools Command Prompt"? (Normal CMD won't work because it needs `dumpbin`).

*   **"Could NOT find LibObs"**:
    *   Make sure `setup-sdk.ps1` finished successfully and `C:\obs-sdk\bin\64bit\obs.lib` exists.

*   **"Qt6_DIR points to mingw_64"**:
    *   Use "MinGW Makefiles" generator in CMake if you are using MinGW, but we strongly recommend MSVC for OBS plugins on Windows.

Good luck!
