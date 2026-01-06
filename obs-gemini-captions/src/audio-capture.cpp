#include "audio-capture.h"
#include "gemini-client.h"
#include <obs.h>
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

void CaptureAudioData(void *param, obs_source_t *source, const struct audio_data *audio_data, bool muted)
{
    if (muted || !isCapturing) return;

    if (!resampler) return; // Should be initialized

    // Optimization: Use thread_local buffer to avoid re-allocation
    // We expect at most approx ratio * input_frames
    // Max frames is usually 1024. 4096 is safe for single channel 16-bit.

    // Size: frames * channels * bytes_per_sample
    // We resize only if needed.

    static thread_local std::vector<uint8_t> buffer_storage;

    uint32_t estimated_out = (uint32_t)((double)audio_data->frames * ((double)SAMPLE_RATE / (double)obs_source_get_sample_rate(source)) + 16);
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

    // Detect Source Format
    uint32_t sample_rate = obs_source_get_sample_rate(currentAudioSource);
    enum speaker_layout speakers = obs_source_get_speaker_layout(currentAudioSource);

    // We assume FLOAT_PLANAR because that is the standard internal format for sources in the audio pipeline.

    struct resample_info src_info;
    src_info.samples_per_sec = sample_rate;
    src_info.format = AUDIO_FORMAT_FLOAT_PLANAR;
    src_info.speakers = speakers;

    struct resample_info dst_info;
    dst_info.samples_per_sec = SAMPLE_RATE;
    dst_info.format = AUDIO_FORMAT_16BIT;
    dst_info.speakers = SPEAKERS_MONO;

    resampler = audio_resampler_create(&dst_info, &src_info);

    if (!resampler) {
        blog(LOG_ERROR, "Failed to create audio resampler");
        obs_source_release(currentAudioSource);
        return false;
    }

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

    if (resampler) {
        audio_resampler_destroy(resampler);
        resampler = nullptr;
    }

    std::lock_guard<std::mutex> lock(bufferMutex);
    audioBuffer.clear();
}
