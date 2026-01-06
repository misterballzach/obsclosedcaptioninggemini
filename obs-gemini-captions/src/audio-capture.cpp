#include "audio-capture.h"
#include "gemini-client.h"
#include <obs.h>
#include <obs-source.h>
#include "audio-resampler.h"
#include <QDebug>
#include <QMutexLocker>
#include <vector>

AudioCapture::AudioCapture(QObject *parent) : QObject(parent), resampler(nullptr) {}

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
        obs_source_add_audio_capture_callback(source, audioCallback, this);
        capturing = true;

        if (resampler) {
            audio_resampler_destroy(resampler);
            resampler = nullptr;
        }

        qDebug() << "Started audio capture on source:" << sourceName;
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
    qDebug() << "Stopped audio capture";
}

void AudioCapture::processAudio(obs_source_t *source, const struct audio_data *data) {
    if (!resampler) {
        uint32_t sourceRate = obs_source_get_sample_rate(source);
        enum speaker_layout sourceLayout = obs_source_get_speaker_layout(source);

        if (sourceRate == 0 || sourceLayout == SPEAKERS_UNKNOWN) {
            return;
        }

        struct resample_info srcInfo;
        srcInfo.samples_per_sec = sourceRate;
        srcInfo.format = AUDIO_FORMAT_FLOAT_PLANAR;
        srcInfo.speakers = sourceLayout;

        struct resample_info dstInfo;
        dstInfo.samples_per_sec = 16000;
        dstInfo.format = AUDIO_FORMAT_16BIT;
        dstInfo.speakers = SPEAKERS_MONO;

        resampler = audio_resampler_create(&dstInfo, &srcInfo);
        if (!resampler) {
            qWarning() << "Failed to create audio resampler";
            return;
        }
    }

    uint8_t *outputData[MAX_AV_PLANES];
    uint32_t outFrames;
    uint64_t ts_offset;

    if (audio_resampler_resample(resampler, outputData, &outFrames, &ts_offset,
                                 (const uint8_t *const *)data->data, data->frames)) {

        int byteSize = outFrames * sizeof(int16_t);
        QByteArray pcmData(reinterpret_cast<const char*>(outputData[0]), byteSize);
        emit audioPacketReady(pcmData);
    }
}

// --- Global Implementation ---

static AudioCapture* g_captureInstance = nullptr;

bool StartAudioCapture(const std::string& sourceName) {
    if (!g_captureInstance) {
        g_captureInstance = new AudioCapture();

        // Connect signal to Gemini Client
        QObject::connect(g_captureInstance, &AudioCapture::audioPacketReady, [](const QByteArray &data) {
            std::vector<int16_t> pcm(data.size() / 2);
            memcpy(pcm.data(), data.constData(), data.size());

            SendAudioToGemini(pcm, 16000);
        });
    }
    g_captureInstance->startCapture(QString::fromStdString(sourceName));
    return true;
}

void StopAudioCapture() {
    if (g_captureInstance) {
        g_captureInstance->stopCapture();
        // We keep the instance alive
    }
}
