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
#include <QThread>

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
}

void GeminiClient::onReplyFinished(QNetworkReply* reply) {
     if (reply->error() == QNetworkReply::NoError) {
        // This is a fire-and-forget audio upload response
        QByteArray response = reply->readAll();
        std::string text = ExtractTextFromJson(response.toStdString());
        if (!text.empty()) {
            blog(LOG_INFO, "Transcribed: %s", text.c_str());
            UpdateCaption(text);
        }
    } else {
        blog(LOG_ERROR, "Gemini API request failed: %s", reply->errorString().toStdString().c_str());
    }
    reply->deleteLater();
}

void GeminiClient::PerformAsyncRequest(const std::string& jsonPayload, std::function<void(std::string)> callback) {
    std::string apiKey = GetGeminiAPIKey();
    if (apiKey.empty()) return;

    QUrl url("https://generativelanguage.googleapis.com/v1beta/models/gemini-1.5-flash:generateContent?key=" + QString::fromStdString(apiKey));
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply* reply = manager->post(request, QByteArray::fromStdString(jsonPayload));

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

    // Async request
    std::string apiKey = GetGeminiAPIKey();
    if (apiKey.empty()) return;

    QUrl url("https://generativelanguage.googleapis.com/v1beta/models/gemini-1.5-flash:generateContent?key=" + QString::fromStdString(apiKey));
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QNetworkReply* reply = manager->post(request, QByteArray::fromStdString(jsonPayload));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onReplyFinished(reply);
    });
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

// We hold a persistent client on the main thread.
static GeminiClient* g_client = nullptr;

// Accessor for other modules
GeminiClient* GetGlobalGeminiClient() {
    if (!g_client) {
        g_client = new GeminiClient();
    }
    return g_client;
}

void SendTextQueryToGemini(const std::string& prompt, std::function<void(std::string)> callback) {
    GeminiClient* client = GetGlobalGeminiClient();
    client->SendTextQuery(prompt, callback);
}
