#pragma once

#include <vector>
#include <cstdint>
#include <string>
#include <functional>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QEventLoop>

// GeminiClient class to handle Qt Network interactions
class GeminiClient : public QObject {
    Q_OBJECT
public:
    explicit GeminiClient(QObject* parent = nullptr);
    ~GeminiClient();

    void SendAudio(const std::vector<int16_t>& pcmData, int sampleRate);
    void SendTextQuery(const std::string& prompt, std::function<void(std::string)> callback);

private:
    QNetworkAccessManager* manager;

    // Helper for sync requests (internal use only)
    std::string PerformSyncRequest(const std::string& jsonPayload);
    // Helper for async requests
    void PerformAsyncRequest(const std::string& jsonPayload, std::function<void(std::string)> callback);
};

// Global accessors (wrappers around the class)
void SendAudioToGemini(const std::vector<int16_t>& pcmData, int sampleRate);
void SendTextQueryToGemini(const std::string& prompt, std::function<void(std::string)> callback);
