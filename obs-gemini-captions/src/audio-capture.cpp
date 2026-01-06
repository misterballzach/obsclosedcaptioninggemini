#include "audio-capture.h"
#include "gemini-client.h"
#include <obs.h>
#include <obs-source.h>
#include "audio-resampler.h"
#include <QDebug>
#include <QMutexLocker>
#include <vector>

// Forward declare helper
GeminiClient* GetGlobalGeminiClient();

AudioCapture::AudioCapture(QObject *parent) : QObject(parent), resampler(nullptr) {
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

        cachedSampleRate = obs_source_get_sample_rate(source);
        cachedSpeakers = obs_source_get_speaker_layout(source);

        qDebug() << "Started audio capture on source:" << sourceName
                 << "Rate:" << cachedSampleRate
                 << "Layout:" << cachedSpeakers;

        obs_source_add_audio_capture_callback(source, audioCallback, this);
        capturing = true;

        if (resampler) {
            audio_resampler_destroy(resampler);
            resampler = nullptr;
        }

        // Clear buffer on start
        audioBuffer.clear();

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
    if (resampler) {
        audio_resampler_destroy(resampler);
        resampler = nullptr;
    }
    capturing = false;
    audioBuffer.clear();
    qDebug() << "Stopped audio capture";
}

void AudioCapture::processAudio(obs_source_t *source, const struct audio_data *data) {
    if (cachedSampleRate == 0 || cachedSpeakers == SPEAKERS_UNKNOWN) {
        return;
    }

    // Lazy init resampler
    if (!resampler) {
        struct resample_info srcInfo;
        srcInfo.samples_per_sec = cachedSampleRate;
        srcInfo.format = AUDIO_FORMAT_FLOAT_PLANAR;
        srcInfo.speakers = cachedSpeakers;

        struct resample_info dstInfo;
        dstInfo.samples_per_sec = TARGET_SAMPLE_RATE;
        dstInfo.format = AUDIO_FORMAT_16BIT;
        dstInfo.speakers = SPEAKERS_MONO;

        resampler = audio_resampler_create(&dstInfo, &srcInfo);
        if (!resampler) {
            return;
        }
    }

    uint8_t *outputData[MAX_AV_PLANES];
    uint32_t outFrames;
    uint64_t ts_offset;

    if (audio_resampler_resample(resampler, outputData, &outFrames, &ts_offset,
                                 (const uint8_t *const *)data->data, data->frames)) {

        // Append to buffer
        const int16_t* pcmSamples = reinterpret_cast<const int16_t*>(outputData[0]);
        // Mutex is already locked? No, processAudio is called from callback, but start/stop use mutex.
        // We should protect the buffer.
        QMutexLocker locker(&mutex);

        audioBuffer.insert(audioBuffer.end(), pcmSamples, pcmSamples + outFrames);

        // Check size
        size_t targetSamples = (TARGET_BUFFER_DURATION_MS * TARGET_SAMPLE_RATE) / 1000;

        if (audioBuffer.size() >= targetSamples) {
            // Buffer full, emit
            int byteSize = audioBuffer.size() * sizeof(int16_t);
            QByteArray pcmData(reinterpret_cast<const char*>(audioBuffer.data()), byteSize);

            emit audioPacketReady(pcmData);

            audioBuffer.clear();
            // Optional: Keep some overlap? For now, clear.
        }
    }
}

// --- Global Implementation ---

static AudioCapture* g_captureInstance = nullptr;

bool StartAudioCapture(const std::string& sourceName) {
    if (!g_captureInstance) {
        g_captureInstance = new AudioCapture();

        // Connect signal to Gemini Client (Main Thread)
        // We need to use QueuedConnection to ensure it runs on the thread where GeminiClient lives
        GeminiClient* client = GetGlobalGeminiClient();

        QObject::connect(g_captureInstance, &AudioCapture::audioPacketReady, client, [client](const QByteArray &data) {
            std::vector<int16_t> pcm(data.size() / 2);
            memcpy(pcm.data(), data.constData(), data.size());

            client->SendAudio(pcm, 16000);
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
