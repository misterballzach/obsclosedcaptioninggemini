#pragma once

#include <vector>
#include <cstdint>
#include <string>
#include <functional>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>

// GeminiClient class to handle Qt Network interactions
class GeminiClient : public QObject {
    Q_OBJECT
public:
    explicit GeminiClient(QObject* parent = nullptr);
    ~GeminiClient();

    void SendAudio(const std::vector<int16_t>& pcmData, int sampleRate);
    void SendTextQuery(const std::string& prompt, std::function<void(std::string)> callback);

private slots:
    void onReplyFinished(QNetworkReply* reply);

private:
    QNetworkAccessManager* manager;

    // Helper for async requests
    void PerformAsyncRequest(const std::string& jsonPayload, std::function<void(std::string)> callback);
};

// Global accessors (wrappers around the class)
void SendAudioToGemini(const std::vector<int16_t>& pcmData, int sampleRate);
void SendTextQueryToGemini(const std::string& prompt, std::function<void(std::string)> callback);
