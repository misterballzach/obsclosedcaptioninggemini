#include "audio-capture.h"
#include "gemini-client.h"
#include <obs.h>
#include <media-io/audio-resampler.h>
#include <media-io/audio-io.h>
#include <vector>
#include <mutex>
#include <thread>
#include <atomic>
#include <cmath>

static obs_source_t *currentAudioSource = nullptr;
static std::vector<int16_t> audioBuffer;
static std::mutex bufferMutex;
static std::atomic<bool> isCapturing(false);
static std::thread processingThread;

// 16-bit PCM, 16kHz, Mono
const size_t SAMPLE_RATE = 16000;
const size_t CHANNELS = 1;
const size_t CHUNK_DURATION_MS = 3000; // 3 seconds
const size_t SAMPLES_PER_CHUNK = (SAMPLE_RATE * CHUNK_DURATION_MS) / 1000;
const size_t MAX_BUFFER_SIZE = SAMPLES_PER_CHUNK * 5; // 15 seconds max buffer to prevent OOM

// Global resampler
static audio_resampler_t *resampler = nullptr;
static std::mutex resamplerMutex;

void CaptureAudioData(void *param, obs_source_t *source, const struct audio_data *audio_data, bool muted)
{
    if (muted || !isCapturing) return;

    // LAZY INITIALIZATION
    // We do this here because we need the exact sample rate and speakers from the callback data
    // and older OBS API functions like obs_source_get_sample_rate might be unavailable or unreliable.
    {
        std::lock_guard<std::mutex> lock(resamplerMutex);
        if (!resampler) {
            struct resample_info src_info;
            src_info.samples_per_sec = audio_data->samples_per_sec;
            src_info.format = AUDIO_FORMAT_FLOAT_PLANAR; // Standard for OBS sources
            src_info.speakers = audio_data->speakers;

            struct resample_info dst_info;
            dst_info.samples_per_sec = SAMPLE_RATE;
            dst_info.format = AUDIO_FORMAT_16BIT;
            dst_info.speakers = SPEAKERS_MONO;

            resampler = audio_resampler_create(&dst_info, &src_info);

            if (!resampler) {
                blog(LOG_ERROR, "Failed to create audio resampler inside callback");
                return;
            }
            blog(LOG_INFO, "Audio Resampler created: %d Hz -> %d Hz", src_info.samples_per_sec, SAMPLE_RATE);
        }
    }

    if (!resampler) return;

    // Optimization: Use thread_local buffer to avoid re-allocation
    static thread_local std::vector<uint8_t> buffer_storage;

    // Calculate input sample rate safely
    // audio_data->samples_per_sec is reliable here
    uint32_t estimated_out = (uint32_t)((double)audio_data->frames * ((double)SAMPLE_RATE / (double)audio_data->samples_per_sec) + 16);
    size_t req_size = estimated_out * CHANNELS * sizeof(int16_t);

    if (buffer_storage.size() < req_size) {
        buffer_storage.resize(req_size);
    }

    uint8_t *resample_buffer[MAX_AV_PLANES];
    resample_buffer[0] = buffer_storage.data();
    for(int i=1; i<MAX_AV_PLANES; i++) resample_buffer[i] = nullptr;

    uint32_t out_frames = estimated_out;
    uint64_t ts_offset;

    bool success = audio_resampler_resample(resampler, resample_buffer, &out_frames, &ts_offset,
                                            audio_data->data, audio_data->frames);

    if (success && out_frames > 0) {
        std::lock_guard<std::mutex> lock(bufferMutex);

        // Prevent buffer bloat
        if (audioBuffer.size() > MAX_BUFFER_SIZE) {
            audioBuffer.erase(audioBuffer.begin(), audioBuffer.begin() + out_frames);
        }

        int16_t *pcm = (int16_t*)resample_buffer[0];
        for (uint32_t i = 0; i < out_frames; i++) {
            audioBuffer.push_back(pcm[i]);
        }
    }
}

void ProcessLoop()
{
    while (isCapturing) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        std::vector<int16_t> chunk;
        {
            std::lock_guard<std::mutex> lock(bufferMutex);
            if (audioBuffer.size() >= SAMPLES_PER_CHUNK) {
                // Extract chunk
                chunk.assign(audioBuffer.begin(), audioBuffer.begin() + SAMPLES_PER_CHUNK);
                audioBuffer.erase(audioBuffer.begin(), audioBuffer.begin() + SAMPLES_PER_CHUNK);
            }
        }

        if (!chunk.empty()) {
            SendAudioToGemini(chunk, SAMPLE_RATE);
        }
    }
}

bool StartAudioCapture(const std::string& sourceName)
{
    if (isCapturing) return true;

    currentAudioSource = obs_get_source_by_name(sourceName.c_str());
    if (!currentAudioSource) return false;

    // We defer resampler creation to the first callback execution
    // because that is where we get the true source format reliably.

    obs_source_add_audio_capture_callback(currentAudioSource, CaptureAudioData, nullptr);

    isCapturing = true;
    processingThread = std::thread(ProcessLoop);

    return true;
}

void StopAudioCapture()
{
    if (!isCapturing) return;

    isCapturing = false;
    if (processingThread.joinable()) {
        processingThread.join();
    }

    if (currentAudioSource) {
        obs_source_remove_audio_capture_callback(currentAudioSource, CaptureAudioData, nullptr);
        obs_source_release(currentAudioSource);
        currentAudioSource = nullptr;
    }

    // Destroy resampler
    {
        std::lock_guard<std::mutex> lock(resamplerMutex);
        if (resampler) {
            audio_resampler_destroy(resampler);
            resampler = nullptr;
        }
    }

    std::lock_guard<std::mutex> lock(bufferMutex);
    audioBuffer.clear();
}
