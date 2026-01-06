#pragma once

#include <QObject>
#include <QMutex>
#include <obs.h>
#include <string>
#include <vector>

struct audio_data;
struct audio_resampler;

class AudioCapture : public QObject {
    Q_OBJECT
public:
    explicit AudioCapture(QObject *parent = nullptr);
    ~AudioCapture();

    void startCapture(const QString &sourceName);
    void stopCapture();

signals:
    void audioPacketReady(const QByteArray &pcmData);

private:
    static void audioCallback(void *param, obs_source_t *source, const struct audio_data *audio_data, bool muted);
    void processAudio(obs_source_t *source, const struct audio_data *data);

    obs_source_t *currentAudioSource = nullptr;
    QString currentSourceName;
    audio_resampler *resampler = nullptr;
    bool capturing = false;
    QMutex mutex;

    // Cached audio format
    uint32_t cachedSampleRate = 0;
    enum speaker_layout cachedSpeakers = SPEAKERS_UNKNOWN;

    // Buffer for accumulation
    std::vector<int16_t> audioBuffer;
    const size_t TARGET_BUFFER_DURATION_MS = 5000; // 5 seconds
    const int TARGET_SAMPLE_RATE = 16000;
};

// Global interface
bool StartAudioCapture(const std::string& sourceName);
void StopAudioCapture();
