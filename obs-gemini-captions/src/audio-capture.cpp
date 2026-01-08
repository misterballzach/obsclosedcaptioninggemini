#include "audio-capture.h"
#include "gemini-client.h"
#include <obs.h>
#include <obs-source.h>
#include <media-io/audio-io.h>
#include <QDebug>
#include <QMutexLocker>
#include <vector>

// Forward declare helper
GeminiClient* GetGlobalGeminiClient();

AudioCapture::AudioCapture(QObject *parent) : QObject(parent) {
    // Reserve buffer space (16kHz * 5s)
    audioBuffer.reserve(16000 * 5);
}

AudioCapture::~AudioCapture() {
    stopCapture();
}

void AudioCapture::audioCallback(void *param, obs_source_t *source, const struct audio_data *audio_data, bool muted) {
    if (muted) return;
    AudioCapture *capture = static_cast<AudioCapture*>(param);
    if (capture) {
        capture->processAudio(source, audio_data);
    }
}

void AudioCapture::startCapture(const QString &sourceName) {
    QMutexLocker locker(&mutex);
    if (capturing) {
        stopCapture();
    }

    currentSourceName = sourceName;
    obs_source_t *source = obs_get_source_by_name(sourceName.toUtf8().constData());
    if (source) {
        currentAudioSource = source;

        // Use Global Audio Info (OBS 32+)
        // Signature: const audio_output_info *audio_output_get_info(const audio_t *audio);
        const struct audio_output_info *info = audio_output_get_info(obs_get_audio());

        if (info) {
            cachedSampleRate = info->samples_per_sec;
            cachedSpeakers = info->speakers;

            qDebug() << "Started audio capture on source:" << sourceName
                     << "Global Rate:" << cachedSampleRate
                     << "Global Layout:" << cachedSpeakers;

            obs_source_add_audio_capture_callback(source, audioCallback, this);
            capturing = true;

            // Clear buffer on start
            audioBuffer.clear();
        } else {
             qWarning() << "Failed to get audio info from OBS";
        }

    } else {
        qDebug() << "Failed to find source:" << sourceName;
    }
}

void AudioCapture::stopCapture() {
    QMutexLocker locker(&mutex);
    if (currentAudioSource) {
        obs_source_remove_audio_capture_callback(currentAudioSource, audioCallback, this);
        obs_source_release(currentAudioSource);
        currentAudioSource = nullptr;
    }
    capturing = false;
    audioBuffer.clear();
    qDebug() << "Stopped audio capture";
}

void AudioCapture::processAudio(obs_source_t *source, const struct audio_data *data) {
    if (cachedSampleRate == 0 || cachedSpeakers == SPEAKERS_UNKNOWN) {
        return;
    }

    // We need to convert Source Rate (Float) -> 16kHz (Int16)
    // Simple Linear Resampler logic
    // NOTE: While libobs has an audio-resampler, headers (media-io/audio-resampler.h)
    // are not always exposed in binary SDKs for plugins.
    // A manual linear resampler is sufficient for speech recognition (16kHz)
    // and avoids dependency hell.

    // NOTE: OBS audio is planar float. data->data[0] is channel 1.
    const float* floatSamples = (const float*)data->data[0];
    uint32_t inputFrames = data->frames;

    const int TARGET_RATE = 16000;

    // Calculate output size
    // Ratio = In / Out
    double ratio = (double)cachedSampleRate / (double)TARGET_RATE;

    // Est output frames
    size_t outputFrames = (size_t)(inputFrames / ratio);
    if (outputFrames == 0) return; // Not enough input

    std::vector<int16_t> resampledSamples;
    resampledSamples.reserve(outputFrames);

    // Simple Linear Interpolation
    for (size_t i = 0; i < outputFrames; ++i) {
        double srcIndex = i * ratio;
        size_t idx0 = (size_t)srcIndex;
        size_t idx1 = idx0 + 1;

        if (idx1 >= inputFrames) idx1 = idx0; // Clamp

        float frac = (float)(srcIndex - idx0);

        float s0 = floatSamples[idx0];
        float s1 = floatSamples[idx1];

        float val = s0 + (s1 - s0) * frac;

        // Clamp and Convert
        if (val > 1.0f) val = 1.0f;
        if (val < -1.0f) val = -1.0f;

        resampledSamples.push_back((int16_t)(val * 32767.0f));
    }

    // Append to buffer
    QMutexLocker locker(&mutex);
    audioBuffer.insert(audioBuffer.end(), resampledSamples.begin(), resampledSamples.end());

    // Check size (Target 5 seconds at 16kHz)
    size_t targetBufferSamples = 5 * TARGET_RATE; // 80000 samples

    if (audioBuffer.size() >= targetBufferSamples) {
        // Emit
        int byteSize = audioBuffer.size() * sizeof(int16_t);
        QByteArray pcmData(reinterpret_cast<const char*>(audioBuffer.data()), byteSize);

        emit audioPacketReady(pcmData, TARGET_RATE);

        audioBuffer.clear();
    }
}

// --- Global Implementation ---

static AudioCapture* g_captureInstance = nullptr;

bool StartAudioCapture(const std::string& sourceName) {
    if (!g_captureInstance) {
        g_captureInstance = new AudioCapture();

        GeminiClient* client = GetGlobalGeminiClient();

        // Update connection signature to match new signal
        QObject::connect(g_captureInstance, &AudioCapture::audioPacketReady, client, [client](const QByteArray &data, int rate) {
            std::vector<int16_t> pcm(data.size() / 2);
            memcpy(pcm.data(), data.constData(), data.size());

            client->SendAudio(pcm, rate);
        }, Qt::QueuedConnection);
    }
    g_captureInstance->startCapture(QString::fromStdString(sourceName));
    return true;
}

void StopAudioCapture() {
    if (g_captureInstance) {
        g_captureInstance->stopCapture();
    }
}
