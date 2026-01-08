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
    // Updated to include sample rate
    void audioPacketReady(const QByteArray &pcmData, int sampleRate);

private:
    static void audioCallback(void *param, obs_source_t *source, const struct audio_data *audio_data, bool muted);
    void processAudio(obs_source_t *source, const struct audio_data *data);

    obs_source_t *currentAudioSource = nullptr;
    QString currentSourceName;
    bool capturing = false;
    QMutex mutex;

    // Cached audio format
    uint32_t cachedSampleRate = 0;
    enum speaker_layout cachedSpeakers = SPEAKERS_UNKNOWN;

    // Resampler
    audio_resampler *resampler = nullptr;

    // Buffer for accumulation
    std::vector<int16_t> audioBuffer;
};

// Global interface
bool StartAudioCapture(const std::string& sourceName);
void StopAudioCapture();
