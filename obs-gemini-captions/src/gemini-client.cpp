#include "gemini-client.h"
#include "plugin-main.h"
#include "caption-output.h"
#include <vector>
#include <string>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <obs.h>
#include <thread>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkRequest>
#include <QUrl>

// Simple Base64 encoder (still useful, or use QByteArray::toBase64)
static const std::string base64_chars =
             "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
             "abcdefghijklmnopqrstuvwxyz"
             "0123456789+/";

std::string base64_encode(unsigned char const* bytes_to_encode, unsigned int in_len) {
  std::string ret;
  int i = 0;
  int j = 0;
  unsigned char char_array_3[3];
  unsigned char char_array_4[4];

  while (in_len--) {
    char_array_3[i++] = *(bytes_to_encode++);
    if (i == 3) {
      char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
      char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
      char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
      char_array_4[3] = char_array_3[2] & 0x3f;

      for(i = 0; (i <4) ; i++)
        ret += base64_chars[char_array_4[i]];
      i = 0;
    }
  }

  if (i) {
    for(j = i; j < 3; j++)
      char_array_3[j] = '\0';

    char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
    char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
    char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
    char_array_4[3] = char_array_3[2] & 0x3f;

    for (j = 0; (j < i + 1); j++)
      ret += base64_chars[char_array_4[j]];

    while((i++ < 3))
      ret += '=';
  }

  return ret;
}

std::string ExtractTextFromJson(const std::string& json) {
    // We can use Qt JSON parser now
    QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(json));
    if (doc.isNull()) return "";

    QJsonObject root = doc.object();
    QJsonArray candidates = root["candidates"].toArray();
    if (candidates.isEmpty()) return "";

    QJsonObject candidate = candidates[0].toObject();
    QJsonObject content = candidate["content"].toObject();
    QJsonArray parts = content["parts"].toArray();
    if (parts.isEmpty()) return "";

    QJsonObject part = parts[0].toObject();
    return part["text"].toString().toStdString();
}

GeminiClient::GeminiClient(QObject* parent) : QObject(parent) {
    manager = new QNetworkAccessManager(this);
}

GeminiClient::~GeminiClient() {
    // QObject cleanup handles manager
}

std::string GeminiClient::PerformSyncRequest(const std::string& jsonPayload) {
    std::string apiKey = GetGeminiAPIKey();
    if (apiKey.empty()) return "";

    QUrl url("https://generativelanguage.googleapis.com/v1beta/models/gemini-1.5-flash:generateContent?key=" + QString::fromStdString(apiKey));
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply* reply = manager->post(request, QByteArray::fromStdString(jsonPayload));

    // Wait for reply synchronously using local event loop
    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    std::string result = "";
    if (reply->error() == QNetworkReply::NoError) {
        QByteArray response = reply->readAll();
        result = ExtractTextFromJson(response.toStdString());
    } else {
        blog(LOG_ERROR, "Gemini API request failed: %s", reply->errorString().toStdString().c_str());
    }

    reply->deleteLater();
    return result;
}

void GeminiClient::PerformAsyncRequest(const std::string& jsonPayload, std::function<void(std::string)> callback) {
    std::string apiKey = GetGeminiAPIKey();
    if (apiKey.empty()) return;

    QUrl url("https://generativelanguage.googleapis.com/v1beta/models/gemini-1.5-flash:generateContent?key=" + QString::fromStdString(apiKey));
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply* reply = manager->post(request, QByteArray::fromStdString(jsonPayload));

    // Connect cleanup and callback
    connect(reply, &QNetworkReply::finished, [reply, callback]() {
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray response = reply->readAll();
            std::string text = ExtractTextFromJson(response.toStdString());
            if (callback) callback(text);
        } else {
            blog(LOG_ERROR, "Gemini API request failed: %s", reply->errorString().toStdString().c_str());
        }
        reply->deleteLater();
    });
}

void GeminiClient::SendAudio(const std::vector<int16_t>& pcmData, int sampleRate) {
    // Build JSON payload (same logic as before)
    std::vector<uint8_t> wavData;
    uint32_t totalDataLen = pcmData.size() * 2;
    uint32_t riffChunkSize = 36 + totalDataLen;
    uint32_t byteRate = sampleRate * 1 * 2;

    wavData.insert(wavData.end(), {'R', 'I', 'F', 'F'});
    wavData.insert(wavData.end(), (uint8_t*)&riffChunkSize, (uint8_t*)&riffChunkSize + 4);
    wavData.insert(wavData.end(), {'W', 'A', 'V', 'E'});
    wavData.insert(wavData.end(), {'f', 'm', 't', ' '});
    uint32_t fmtChunkSize = 16;
    wavData.insert(wavData.end(), (uint8_t*)&fmtChunkSize, (uint8_t*)&fmtChunkSize + 4);
    uint16_t audioFormat = 1;
    wavData.insert(wavData.end(), (uint8_t*)&audioFormat, (uint8_t*)&audioFormat + 2);
    uint16_t numChannels = 1;
    wavData.insert(wavData.end(), (uint8_t*)&numChannels, (uint8_t*)&numChannels + 2);
    uint32_t sampleRate32 = sampleRate;
    wavData.insert(wavData.end(), (uint8_t*)&sampleRate32, (uint8_t*)&sampleRate32 + 4);
    wavData.insert(wavData.end(), (uint8_t*)&byteRate, (uint8_t*)&byteRate + 4);
    uint16_t blockAlign = 2;
    wavData.insert(wavData.end(), (uint8_t*)&blockAlign, (uint8_t*)&blockAlign + 2);
    uint16_t bitsPerSample = 16;
    wavData.insert(wavData.end(), (uint8_t*)&bitsPerSample, (uint8_t*)&bitsPerSample + 2);
    wavData.insert(wavData.end(), {'d', 'a', 't', 'a'});
    wavData.insert(wavData.end(), (uint8_t*)&totalDataLen, (uint8_t*)&totalDataLen + 4);

    const uint8_t* pcmBytes = (const uint8_t*)pcmData.data();
    wavData.insert(wavData.end(), pcmBytes, pcmBytes + totalDataLen);

    std::string base64Audio = base64_encode(wavData.data(), wavData.size());

    // Construct JSON using Qt JSON (cleaner)
    QJsonObject root;
    QJsonArray contents;
    QJsonObject contentItem;
    QJsonArray parts;

    QJsonObject textPart;
    textPart["text"] = "Transcribe the following audio to text. Output only the transcribed text, nothing else.";
    parts.append(textPart);

    QJsonObject audioPart;
    QJsonObject inlineData;
    inlineData["mimeType"] = "audio/wav";
    inlineData["data"] = QString::fromStdString(base64Audio);
    audioPart["inlineData"] = inlineData;
    parts.append(audioPart);

    contentItem["parts"] = parts;
    contents.append(contentItem);
    root["contents"] = contents;

    QJsonDocument doc(root);
    std::string jsonPayload = doc.toJson(QJsonDocument::Compact).toStdString();

    // Perform Sync Request (since we are in a worker thread)
    std::string text = PerformSyncRequest(jsonPayload);
    if (!text.empty()) {
        blog(LOG_INFO, "Transcribed: %s", text.c_str());
        UpdateCaption(text);
    }
}

void GeminiClient::SendTextQuery(const std::string& prompt, std::function<void(std::string)> callback) {
    QJsonObject root;
    QJsonArray contents;
    QJsonObject contentItem;
    QJsonArray parts;

    QJsonObject textPart;
    textPart["text"] = QString::fromStdString(prompt);
    parts.append(textPart);

    contentItem["parts"] = parts;
    contents.append(contentItem);
    root["contents"] = contents;

    QJsonDocument doc(root);
    std::string jsonPayload = doc.toJson(QJsonDocument::Compact).toStdString();

    PerformAsyncRequest(jsonPayload, callback);
}


// --- Global Wrappers ---

// We need to manage the GeminiClient instance.
// Since SendAudioToGemini is called from a worker thread (ProcessLoop), and that thread
// doesn't have a built-in QEventLoop, creating a QObject (GeminiClient) on it is fine
// IF we spin a local loop, which we do in PerformSyncRequest.

// However, we want to avoid recreating the QNetworkAccessManager every 3 seconds.
// thread_local storage is a good place for it.

void SendAudioToGemini(const std::vector<int16_t>& pcmData, int sampleRate) {
    // This runs in 'processingThread'
    static thread_local GeminiClient client;
    client.SendAudio(pcmData, sampleRate);
}

void SendTextQueryToGemini(const std::string& prompt, std::function<void(std::string)> callback) {
    // This is called from the UI thread (TwitchBot logic) usually.
    // Or we can spawn a detached thread.
    // The previous implementation detached a thread.
    // QNetworkAccessManager is async, so we don't need to detach if we use the main thread event loop.
    // However, TwitchBot::processLine calls this.

    // We can use a static client on the main thread?
    // Note: TwitchBot lives on main thread.

    // Let's execute this on the main thread to be safe with QObjects
    // If we are not on main thread, invoke.

    // Actually, simply creating a new QNAM for each text query is fine as they are infrequent.
    // Or better, make TwitchBot own a GeminiClient instance?
    // For simplicity of the global API provided in headers:

    // We need to ensure QNAM lives on a thread with an event loop (Main thread has one).
    // If SendTextQueryToGemini is called from a thread without loop, it won't work async.
    // TwitchBot logic runs on main thread (socket signals). So we are good.

    static GeminiClient* mainThreadClient = nullptr;
    if (!mainThreadClient) {
        // Leak it intentionally or manage properly. Global is fine for plugin scope.
        mainThreadClient = new GeminiClient();
        // Note: QObject needs parent or thread affinity.
        // If this function is called from main thread first time, it binds to main thread.
    }

    mainThreadClient->SendTextQuery(prompt, callback);
}
