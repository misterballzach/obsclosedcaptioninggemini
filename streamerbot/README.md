# Streamer.bot Gemini Captions

This is a **C# Code Action** for Streamer.bot that provides closed captioning using Google Gemini.
It captures your microphone audio directly (no OBS plugin required), sends it to Gemini, and then sends the captions to OBS (via WebSocket) and optionally Twitch chat.

## Prerequisites

1.  **Streamer.bot** (v0.1.8 or later, v0.2.x recommended).
2.  **OBS Studio** with OBS WebSocket enabled (Tools -> WebSocket Server Settings).
3.  **Google Gemini API Key**.

## Installation

1.  Open **Streamer.bot**.
2.  Go to the **Actions** tab.
3.  Right-click -> **Add**. Name it `Gemini Captions Start`.
4.  In the `Sub-Actions` pane, right-click -> **Core** -> **C#** -> **Execute C# Code**.
5.  A code editor window will open.
6.  **Copy and Paste** the content of `GeminiCaptionsAction.cs` into this window.
7.  **Add References:**
    *   Right-click in the editor -> **Add Reference** (or look for the References tab).
    *   Add `System.Net.Http.dll` (usually available in the list or browse to .NET framework).
    *   Add `System.Core.dll` (if not present).
    *   Add `System.Windows.Forms.dll` (optional, usually not needed for this script but good to have).
    *   Ensure `Newtonsoft.Json.dll` is referenced (Streamer.bot usually includes this by default).
8.  **Compile** to check for errors. Click **Save and Compile**.

## Configuration

1.  In Streamer.bot, go to **Settings** -> **Global Variables**.
2.  Add a new variable:
    *   Name: `GeminiKey`
    *   Value: `YOUR_GOOGLE_GEMINI_API_KEY`
    *   Persisted: Yes

## Usage

1.  Run the `Gemini Captions Start` action (you can bind it to a voice command, hotkey, or Stream Deck button).
    *   **First Run:** Starts capturing audio.
    *   **Second Run:** Stops capturing audio.
2.  Check the **Action Log** in Streamer.bot to see status (`[Gemini] Starting...`).
3.  Speak into your default microphone.
4.  Captions should appear in OBS (if you have a Closed Caption source or view) and in the log.

## Notes

*   **Audio Device:** The script uses the Windows Default Recording Device (WaveIn ID 0). Ensure your mic is set as default in Windows Sound Settings.
*   **Latency:** Audio is buffered in 5-second chunks. This means captions appear every ~5 seconds.
