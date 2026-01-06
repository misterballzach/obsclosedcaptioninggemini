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
        *   **IF YOU MUST USE MINGW:** See the "MinGW Users" section below.
        *   Check **Qt Network** (often included in base, but double check).
    *   Remember where you installed it! Usually `C:\Qt`.

4.  **OBS Studio SDK (Important!)**
    *   You CANNOT just use the "Source Code" zip file from GitHub. It is missing the `.lib` files needed to compile plugins.
    *   **You need the OBS Studio SDK.**
    *   **Where to get it:**
        *   Go to the [OBS GitHub Actions page](https://github.com/obsproject/obs-studio/actions).
        *   Click on the latest "CI" workflow run that passed (green checkmark).
        *   Scroll down to **Artifacts**.
        *   Download `windows-x64-sdk` (or similar name).
        *   **Extract this folder** to `C:\obs-sdk`.

---

## Part 2: The Scary Part (Building the Plugin)

### Standard Method (MSVC Qt)

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
    *   *Note:* You might see red text about `pthread` or `Vulkan`. **Ignore this.** As long as it says "Configuring done" at the bottom, you are safe.
7.  **Generate** -> **Open Project** -> Build in Visual Studio.

---

### MinGW Users (If you can't install MSVC Qt)

**WARNING:** If you use MinGW Qt, you CANNOT use Visual Studio to build. You must use the MinGW compiler (`gcc`/`g++`).
**WARNING 2:** A plugin built with MinGW might NOT load in standard OBS Studio (which uses MSVC). It might crash. Proceed at your own risk.

1.  **Open CMake (cmake-gui)**.
    *   **IMPORTANT:** If you already ran Configure with "Visual Studio", you must click **File -> Delete Cache** first.
2.  **Configure:**
    *   Generator: Select **"MinGW Makefiles"**. (Do NOT select Visual Studio).
    *   Select "Use default native compilers" (if you have MinGW in your PATH).
3.  **Fix Errors:**
    *   Set `LIBOBS_INCLUDE_DIR` and `LIBOBS_LIB` as usual.
    *   Set `Qt6_DIR` to your MinGW Qt folder (e.g., `C:\Qt\6.10.1\mingw_64\lib\cmake\Qt6`).
4.  **Generate**.
5.  **Build:**
    *   You cannot click "Open Project".
    *   Open a command prompt (cmd) in your `build` folder.
    *   Type `mingw32-make` (or just `make` if setup that way).
    *   This will create the `.dll` file.

---

## Part 3: Putting it in OBS

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

## Part 4: How to Use It

1.  **Get a Gemini API Key** from [Google AI Studio](https://aistudio.google.com/).
2.  **Start OBS** -> **Tools** -> **Gemini Captions**.
3.  Enter API Key and select Audio Source.
4.  Click **Start Captioning**.

---

## Troubleshooting

*   **"Configuring done" but I see red text!**:
    *   If the red text is `pthread` failed or `Vulkan` not found, **Ignore it**. This is normal on Windows.
    *   If the text is `Could NOT find LibObs`, you need to fix your paths.

*   **"CRITICAL CONFIGURATION ERROR: You selected a SOURCE CODE file"**:
    *   You pointed CMake to `obs.h` or `obs.c` instead of `obs.lib`.
    *   Download the **OBS SDK** (not Source Code) and point to `bin/64bit/obs.lib`.

*   **"Qt6_DIR points to mingw_64"**: Use "MinGW Makefiles" generator in CMake.

Good luck!
