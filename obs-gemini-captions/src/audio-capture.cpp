#include "audio-capture.h"
#include "gemini-client.h"
#include <obs.h>
#include <obs-source.h>
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

        cachedSampleRate = obs_source_get_sample_rate(source);
        cachedSpeakers = obs_source_get_speaker_layout(source);

        qDebug() << "Started audio capture on source:" << sourceName
                 << "Rate:" << cachedSampleRate
                 << "Layout:" << cachedSpeakers;

        obs_source_add_audio_capture_callback(source, audioCallback, this);
        capturing = true;

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
    capturing = false;
    audioBuffer.clear();
    qDebug() << "Stopped audio capture";
}

void AudioCapture::processAudio(obs_source_t *source, const struct audio_data *data) {
    if (cachedSampleRate == 0 || cachedSpeakers == SPEAKERS_UNKNOWN) {
        return;
    }

    // Naive Float to Int16 Conversion (No Resampling for now)
    // We assume the Gemini Client will handle the sample rate in header, or the API is tolerant.
    // Ideally we should resample, but we removed libobs resampler usage.
    // For now, we will just take the first channel and convert to int16.

    // NOTE: OBS audio is planar float. data->data[0] is channel 1.
    const float* floatSamples = (const float*)data->data[0];
    size_t frames = data->frames;

    std::vector<int16_t> newSamples(frames);
    for (size_t i = 0; i < frames; i++) {
        float sample = floatSamples[i];
        if (sample > 1.0f) sample = 1.0f;
        if (sample < -1.0f) sample = -1.0f;
        newSamples[i] = (int16_t)(sample * 32767.0f);
    }

    // Append to buffer
    QMutexLocker locker(&mutex);
    audioBuffer.insert(audioBuffer.end(), newSamples.begin(), newSamples.end());

    // Check size (Target 5 seconds approx)
    // If we are at 44.1kHz, 5s = 220500 samples
    size_t targetSamples = (size_t)(5.0 * cachedSampleRate);

    if (audioBuffer.size() >= targetSamples) {
        // Emit
        int byteSize = audioBuffer.size() * sizeof(int16_t);
        QByteArray pcmData(reinterpret_cast<const char*>(audioBuffer.data()), byteSize);

        // We need to pass the Sample Rate so the WAV header is correct!
        // The signal only passes byte array. We should include rate?
        // Or we assume the receiver knows?
        // Let's modify the emission to assume cachedSampleRate.
        // But the signal signature is fixed in header.
        // We will just emit. The lambda in StartAudioCapture knows the rate? No, it's a static lambda.
        // We need to pass rate.
        // For minimal changes: We will re-use the signal but maybe prepend metadata? No.
        // Let's just update the signal in header to include rate, or pass it.
        // Ah, audioPacketReady(QByteArray) is what we have.
        // We should add rate to signal.

        emit audioPacketReady(pcmData, (int)cachedSampleRate);

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
