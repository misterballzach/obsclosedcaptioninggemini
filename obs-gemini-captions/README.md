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
    *   This is the hardest part. You need an account.
    *   Download the "Online Installer": [https://www.qt.io/download-qt-installer](https://www.qt.io/download-qt-installer)
    *   Run it, log in.
    *   In the "Select Components" screen:
        *   Expand **Qt 6.x.x** (pick the latest 6.x version, e.g., 6.6 or 6.7).
        *   **CRITICAL:** You must check **MSVC 2019 64-bit** (or MSVC 2022).
        *   **DO NOT** select "MinGW". If you use MinGW with Visual Studio, it will break.
        *   Check **Qt Network** (often included in base, but double check).
    *   Remember where you installed it! Usually `C:\Qt`.

4.  **OBS Studio Libraries (The "libobs" stuff)**
    *   CMake needs to know where OBS is to build the plugin.
    *   **Option A (Hard):** Build OBS Studio from source.
    *   **Option B (Easy):** You still need the source code.
        *   Go to: [https://github.com/obsproject/obs-studio](https://github.com/obsproject/obs-studio)
        *   Click **Code** -> **Download ZIP**. Extract to `C:\obs-studio`.
        *   **Important:** You also need the "libs" (compiled files).
        *   If you can't build OBS, you might be stuck. But we will try to just point CMake to the headers.
        *   *Tip:* Sometimes you can grab the "CI Artifacts" (obs-studio-x64-dev.zip) from the OBS GitHub Actions page if you have a GitHub account. This is the "proper" SDK. Extract it to `C:\obs-sdk`.

---

## Part 2: The Scary Part (Building the Plugin)

1.  **Open CMake (cmake-gui)**
    *   Press Start, type `CMake`, run it.

2.  **Tell it where the code is:**
    *   **Where is the source code:** Browse to the folder where `CMakeLists.txt` is (this folder you are reading this in).
    *   **Where to build the binaries:** Create a new folder inside this one called `build` and select it.

3.  **Click "Configure"**
    *   A popup appears. Select **Visual Studio 17 2022**.
    *   Platform: **x64**.
    *   Click Finish.

4.  **Fix the Errors (The Red Text)**
    *   **It is NORMAL to see red text and errors the first time!** Do not panic.
    *   Look at the screenshot you sent. You see `LIBOBS_INCLUDE_DIR` and `LIBOBS_LIB` are red and say `NOTFOUND`. This means CMake doesn't know where you put OBS.
    *   **Fix LibObs:**
        *   Click the empty/red box next to **LIBOBS_INCLUDE_DIR**. Click the "..." button.
        *   Browse to the folder `libobs` inside where you downloaded OBS (e.g., `C:\obs-studio\libobs`). It should have `obs.h` inside it.
        *   Click the box next to **LIBOBS_LIB**.
        *   **IMPORTANT:** This must point to a FILE (e.g., `obs.lib`), NOT a FOLDER. If you point it to a folder, CMake might say "Configuring done" but the build will fail later.
    *   **Fix ObsFrontendApi:**
        *   If `OBS_FRONTEND_API_INCLUDE_DIR` is red/not found:
            *   Browse to `C:\obs-studio\UI\obs-frontend-api` (Source Code) OR `C:\obs-sdk\include` (SDK). Look for `obs-frontend-api.h`.
        *   If `OBS_FRONTEND_API_LIB` is red/not found:
            *   Browse to `C:\obs-studio\build\UI\obs-frontend-api\Release\obs-frontend-api.lib` (Build) OR `C:\obs-sdk\bin\64bit\obs-frontend-api.lib` (SDK).
    *   **Fix Qt6:**
        *   If `Qt6_DIR` is red, browse to `C:\Qt\6.x.x\msvc2019_64\lib\cmake\Qt6`.
        *   **WARNING:** If this path says `mingw`, STOP. You installed the wrong Qt version. See "Part 1".
    *   **Click Configure again** until the red text turns white and the error at the bottom goes away.

5.  **Click "Generate"**
    *   If it says "Generating done", you won!

6.  **Click "Open Project"**
    *   This opens Visual Studio.

7.  **Compile**
    *   At the top toolbar, change `Debug` to `Release`.
    *   On the right side "Solution Explorer", right-click **obs-gemini-captions** and select **Build**.
    *   If it says "Build: 1 succeeded", you are a genius.

---

## Part 3: Putting it in OBS

You built it! Now you have to install it manually.

1.  **Find the `.dll` file**
    *   Go to your `build/Release` folder.
    *   Find `obs-gemini-captions.dll`.
    *   Copy it.

2.  **Go to your OBS Install Folder**
    *   Usually `C:\Program Files\obs-studio`.
    *   Go to `obs-plugins` -> `64bit`.
    *   **Paste** the `.dll` file here.

3.  **Install the Data (Language files)**
    *   Go back to the source code folder (where this README is).
    *   Copy the `data` folder.
    *   Go to `C:\Program Files\obs-studio\data\obs-plugins`.
    *   Create a folder named `obs-gemini-captions`.
    *   Paste the content of `data` inside so it looks like:
        `C:\Program Files\obs-studio\data\obs-plugins\obs-gemini-captions\locale\en-US.ini`

---

## Part 4: How to Use It

1.  **Get a Gemini API Key**
    *   Go to [Google AI Studio](https://aistudio.google.com/).
    *   Click "Get API Key". Copy it.

2.  **Get Twitch Info (Optional)**
    *   **Username:** Your twitch username.
    *   **Token:** Go to a site like [twitchapps.com/tmi](https://twitchapps.com/tmi/) to get an "oauth token". It looks like `oauth:xyz123...`.
    *   **Channel:** The channel you want the bot to talk in (e.g., your username).

3.  **Start OBS**
    *   Go to the top menu: **Tools** -> **Gemini Captions**.
    *   Paste your API Key.
    *   Select your **Audio Source** (like your Mic).
    *   (Optional) Paste your Twitch info.
    *   Click **Start Captioning**.

4.  **See the Magic**
    *   **Captions:** Will appear in the "Dock". Go to **View -> Docks -> Gemini Captions** if you don't see it.
    *   **Twitch:** People can type `!gemini Tell me a joke` in your chat, and the bot will reply.
    *   **Closed Captions:** Viewers on Twitch can click the "CC" button on your stream video player to see subtitles.

---

## Common Mistakes & Troubleshooting

*   **"Qt6_DIR points to mingw_64"**:
    *   **Problem:** You installed the "MinGW" version of Qt, but you are using Visual Studio (MSVC). They are enemies.
    *   **Fix:** Uninstall MinGW Qt. Install **MSVC 2019 64-bit** Qt. Update the path in CMake.

*   **"LIBOBS_LIB points to a folder"**:
    *   **Problem:** CMake shows "Configuring done", but the build fails with link errors.
    *   **Fix:** In CMake, `LIBOBS_LIB` must point to a file ending in `.lib` (e.g., `obs.lib`), not just the folder it's in.

*   **"Could NOT find WrapVulkanHeaders"**:
    *   **Status:** Ignore this. It's a warning from Qt. It won't stop the build.

*   **"It crashes!"**: You probably didn't copy the `locale` folder correctly. OBS hates it when plugins don't have text files.
*   **"Twitch bot not working!"**: Make sure your token starts with `oauth:` and is valid.

Good luck!
