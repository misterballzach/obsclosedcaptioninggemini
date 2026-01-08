using System;
using System.Collections.Generic;
using System.Net.Http;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Runtime.InteropServices;
using System.IO;
using Newtonsoft.Json;
using Newtonsoft.Json.Linq;

// STREAMER.BOT C# CODE ACTION
// ---------------------------------------------------------------
// Name: Gemini Captions
// Description: Captures audio, sends to Gemini, captions to OBS/Twitch.
// ---------------------------------------------------------------

public class CPHInline
{
    // CONFIGURATION
    private const int SAMPLE_RATE = 16000;
    private const int CHANNELS = 1;
    private const int BUFFER_MS = 100; // Chunk size
    private const int SEND_INTERVAL_MS = 5000; // Send every 5 seconds

    // GLOBALS
    private static bool _isRecording = false;
    private static Thread _recordingThread;
    private static HttpClient _httpClient = new HttpClient();

    // WinMM API
    [DllImport("winmm.dll")]
    public static extern int waveInOpen(out IntPtr hWaveIn, int uDeviceID, ref WaveFormat lpFormat, WaveDelegate dwCallback, IntPtr dwInstance, int dwFlags);
    [DllImport("winmm.dll")]
    public static extern int waveInPrepareHeader(IntPtr hWaveIn, IntPtr lpWaveInHdr, int uSize);
    [DllImport("winmm.dll")]
    public static extern int waveInAddBuffer(IntPtr hWaveIn, IntPtr lpWaveInHdr, int uSize);
    [DllImport("winmm.dll")]
    public static extern int waveInStart(IntPtr hWaveIn);
    [DllImport("winmm.dll")]
    public static extern int waveInStop(IntPtr hWaveIn);
    [DllImport("winmm.dll")]
    public static extern int waveInReset(IntPtr hWaveIn);
    [DllImport("winmm.dll")]
    public static extern int waveInClose(IntPtr hWaveIn);
    [DllImport("winmm.dll")]
    public static extern int waveInUnprepareHeader(IntPtr hWaveIn, IntPtr lpWaveInHdr, int uSize);

    public delegate void WaveDelegate(IntPtr hWaveIn, int uMsg, IntPtr dwInstance, IntPtr dwParam1, IntPtr dwParam2);

    [StructLayout(LayoutKind.Sequential)]
    public struct WaveFormat
    {
        public short wFormatTag;
        public short nChannels;
        public int nSamplesPerSec;
        public int nAvgBytesPerSec;
        public short nBlockAlign;
        public short wBitsPerSample;
        public short cbSize;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct WaveHdr
    {
        public IntPtr lpData;
        public int dwBufferLength;
        public int dwBytesRecorded;
        public int dwUser;
        public int dwFlags;
        public int dwLoops;
        public IntPtr lpNext;
        public int reserved;
    }

    private const int MM_WIM_DATA = 0x3C0;
    private const int CALLBACK_FUNCTION = 0x00030000;

    // State
    private static IntPtr _hWaveIn;
    private static List<byte> _audioBuffer = new List<byte>();
    private static WaveDelegate _callbackDelegate; // Keep ref to prevent GC
    private static object _lock = new object();

    public bool Execute()
    {
        string apiKey = CPH.GetGlobalVar<string>("GeminiKey");
        if (string.IsNullOrEmpty(apiKey))
        {
            CPH.LogInfo("[Gemini] API Key not set! Set global var 'GeminiKey'.");
            return false;
        }

        if (_isRecording)
        {
            StopRecording();
            CPH.LogInfo("[Gemini] Stopped.");
            return true;
        }

        CPH.LogInfo("[Gemini] Starting Capture...");
        StartRecording(apiKey);
        return true;
    }

    private void StartRecording(string apiKey)
    {
        _isRecording = true;

        // Run in thread to not block Streamer.bot
        _recordingThread = new Thread(() => RecordLoop(apiKey));
        _recordingThread.IsBackground = true;
        _recordingThread.Start();
    }

    private void StopRecording()
    {
        _isRecording = false;
        if (_recordingThread != null && _recordingThread.IsAlive)
        {
            // The thread will exit when _isRecording is false
            _recordingThread.Join(1000);
        }
    }

    private void RecordLoop(string apiKey)
    {
        _callbackDelegate = new WaveDelegate(DataCallback);

        WaveFormat fmt = new WaveFormat();
        fmt.wFormatTag = 1; // PCM
        fmt.nChannels = (short)CHANNELS;
        fmt.nSamplesPerSec = SAMPLE_RATE;
        fmt.wBitsPerSample = 16;
        fmt.nBlockAlign = (short)(CHANNELS * (fmt.wBitsPerSample / 8));
        fmt.nAvgBytesPerSec = SAMPLE_RATE * fmt.nBlockAlign;
        fmt.cbSize = 0;

        int res = waveInOpen(out _hWaveIn, 0, ref fmt, _callbackDelegate, IntPtr.Zero, CALLBACK_FUNCTION);
        if (res != 0)
        {
            CPH.LogInfo($"[Gemini] Failed to open mic. Error: {res}");
            _isRecording = false;
            return;
        }

        // Create buffers
        int bufferSize = fmt.nAvgBytesPerSec / (1000 / BUFFER_MS); // 100ms
        for (int i = 0; i < 3; i++)
        {
            AddBuffer(bufferSize);
        }

        waveInStart(_hWaveIn);

        DateTime lastSend = DateTime.Now;

        while (_isRecording)
        {
            Thread.Sleep(100);

            // Check if time to send
            if ((DateTime.Now - lastSend).TotalMilliseconds >= SEND_INTERVAL_MS)
            {
                byte[] chunk;
                lock (_lock)
                {
                    chunk = _audioBuffer.ToArray();
                    _audioBuffer.Clear();
                }

                if (chunk.Length > 0)
                {
                    // Fire and forget send task
                    Task.Run(() => SendToGemini(chunk, apiKey));
                }

                lastSend = DateTime.Now;
            }
        }

        waveInStop(_hWaveIn);
        waveInReset(_hWaveIn);
        waveInClose(_hWaveIn);
    }

    private void AddBuffer(int size)
    {
        WaveHdr hdr = new WaveHdr();
        hdr.dwBufferLength = size;
        hdr.lpData = Marshal.AllocHGlobal(size);
        hdr.dwFlags = 0;

        IntPtr pHdr = Marshal.AllocHGlobal(Marshal.SizeOf(hdr));
        Marshal.StructureToPtr(hdr, pHdr, false);

        waveInPrepareHeader(_hWaveIn, pHdr, Marshal.SizeOf(hdr));
        waveInAddBuffer(_hWaveIn, pHdr, Marshal.SizeOf(hdr));
    }

    private void DataCallback(IntPtr hWaveIn, int uMsg, IntPtr dwInstance, IntPtr dwParam1, IntPtr dwParam2)
    {
        if (uMsg == MM_WIM_DATA)
        {
            if (!_isRecording) return;

            WaveHdr hdr = (WaveHdr)Marshal.PtrToStructure(dwParam1, typeof(WaveHdr));

            if (hdr.dwBytesRecorded > 0)
            {
                byte[] buf = new byte[hdr.dwBytesRecorded];
                Marshal.Copy(hdr.lpData, buf, 0, hdr.dwBytesRecorded);

                lock (_lock)
                {
                    _audioBuffer.AddRange(buf);
                }
            }

            // Reuse buffer
            if (_isRecording)
            {
                waveInAddBuffer(hWaveIn, dwParam1, Marshal.SizeOf(hdr));
            }
            else
            {
                // Cleanup
                waveInUnprepareHeader(hWaveIn, dwParam1, Marshal.SizeOf(hdr));
                Marshal.FreeHGlobal(hdr.lpData);
                Marshal.FreeHGlobal(dwParam1);
            }
        }
    }

    private async Task SendToGemini(byte[] pcmData, string key)
    {
        try
        {
            // Convert PCM to WAV
            byte[] wavData = AddWavHeader(pcmData);
            string base64 = Convert.ToBase64String(wavData);

            var payload = new
            {
                contents = new[]
                {
                    new
                    {
                        parts = new object[]
                        {
                            new { text = "Transcribe this audio. Output only the text." },
                            new
                            {
                                inlineData = new
                                {
                                    mimeType = "audio/wav",
                                    data = base64
                                }
                            }
                        }
                    }
                }
            };

            string json = JsonConvert.SerializeObject(payload);
            var content = new StringContent(json, Encoding.UTF8, "application/json");

            string url = $"https://generativelanguage.googleapis.com/v1beta/models/gemini-1.5-flash:generateContent?key={key}";

            var response = await _httpClient.PostAsync(url, content);
            string respStr = await response.Content.ReadAsStringAsync();

            if (response.IsSuccessStatusCode)
            {
                JObject root = JObject.Parse(respStr);
                string text = root["candidates"]?[0]?["content"]?["parts"]?[0]?["text"]?.ToString();

                if (!string.IsNullOrWhiteSpace(text))
                {
                    text = text.Trim();
                    CPH.LogInfo($"[Gemini] {text}");

                    // Send to OBS (CEA-608)
                    // Note: SendStreamCaption expects 'caption_text'
                    CPH.ObsSendRaw("SendStreamCaption", "{\"caption_text\": \"" + EscapeJson(text) + "\"}");

                    // Optional: Twitch Chat if starts with "Bot:"
                    // CPH.SendMessage(text);
                }
            }
            else
            {
                CPH.LogInfo($"[Gemini] Error: {respStr}");
            }
        }
        catch (Exception ex)
        {
            CPH.LogInfo($"[Gemini] Exception: {ex.Message}");
        }
    }

    private byte[] AddWavHeader(byte[] pcm)
    {
        // Simple WAV Header
        using (MemoryStream ms = new MemoryStream())
        using (BinaryWriter bw = new BinaryWriter(ms))
        {
            bw.Write(new char[] { 'R', 'I', 'F', 'F' });
            bw.Write(36 + pcm.Length);
            bw.Write(new char[] { 'W', 'A', 'V', 'E' });
            bw.Write(new char[] { 'f', 'm', 't', ' ' });
            bw.Write(16);
            bw.Write((short)1); // PCM
            bw.Write((short)CHANNELS);
            bw.Write(SAMPLE_RATE);
            bw.Write(SAMPLE_RATE * CHANNELS * 2);
            bw.Write((short)(CHANNELS * 2));
            bw.Write((short)16);
            bw.Write(new char[] { 'd', 'a', 't', 'a' });
            bw.Write(pcm.Length);
            bw.Write(pcm);
            return ms.ToArray();
        }
    }

    private string EscapeJson(string s)
    {
        return s.Replace("\\", "\\\\").Replace("\"", "\\\"");
    }
}
