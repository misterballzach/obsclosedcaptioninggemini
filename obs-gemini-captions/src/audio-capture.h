#pragma once

#include <QObject>
#include <QMutex>
#include <obs.h>
#include <string>

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
};

// Global interface
bool StartAudioCapture(const std::string& sourceName);
void StopAudioCapture();
