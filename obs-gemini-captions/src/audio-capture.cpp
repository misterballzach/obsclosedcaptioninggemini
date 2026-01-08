#include "audio-capture.h"
#include "gemini-client.h"
#include <obs.h>
#include <obs-source.h>
#include <media-io/audio-io.h>
#include <media-io/audio-resampler.h>
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

        // Use Global Audio Info (OBS 32+)
        const struct audio_output_info *info = audio_output_get_info(obs_get_audio());

        if (info) {
            cachedSampleRate = info->samples_per_sec;
            cachedSpeakers = info->speakers;

            qDebug() << "Started audio capture on source:" << sourceName
                     << "Global Rate:" << cachedSampleRate
                     << "Global Layout:" << cachedSpeakers;

            // Initialize Resampler
            struct resample_info srcInfo;
            srcInfo.samples_per_sec = cachedSampleRate;
            srcInfo.format = AUDIO_FORMAT_FLOAT_PLANAR; // OBS internal format
            srcInfo.speakers = cachedSpeakers;

            struct resample_info dstInfo;
            dstInfo.samples_per_sec = 16000;
            dstInfo.format = AUDIO_FORMAT_16BIT;
            dstInfo.speakers = SPEAKERS_MONO;

            resampler = audio_resampler_create(&dstInfo, &srcInfo);
            if (!resampler) {
                qWarning() << "Failed to create audio resampler";
            }

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
    if (resampler) {
        audio_resampler_destroy(resampler);
        resampler = nullptr;
    }
    capturing = false;
    audioBuffer.clear();
    qDebug() << "Stopped audio capture";
}

void AudioCapture::processAudio(obs_source_t *source, const struct audio_data *data) {
    if (!resampler) return;

    uint8_t *outputData[MAX_AV_PLANES];
    uint32_t outFrames;
    uint64_t ts_offset;

    // Resample using libobs built-in resampler
    if (audio_resampler_resample(resampler, outputData, &outFrames, &ts_offset,
                                 (const uint8_t *const *)data->data, data->frames)) {

        // outputData[0] contains 16-bit 16kHz Mono PCM
        const int16_t* pcmSamples = reinterpret_cast<const int16_t*>(outputData[0]);

        QMutexLocker locker(&mutex);
        audioBuffer.insert(audioBuffer.end(), pcmSamples, pcmSamples + outFrames);

        // Check size (Target 5 seconds at 16kHz)
        size_t targetBufferSamples = 5 * 16000; // 80000 samples

        if (audioBuffer.size() >= targetBufferSamples) {
            // Emit
            int byteSize = audioBuffer.size() * sizeof(int16_t);
            QByteArray pcmData(reinterpret_cast<const char*>(audioBuffer.data()), byteSize);

            emit audioPacketReady(pcmData, 16000);

            audioBuffer.clear();
        }
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
