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

void SendAudioToGemini(const std::vector<int16_t>& pcmData, int sampleRate) {
    // This is called from the Audio Capture thread (from processAudio or buffer processing).
    // We must invoke this on the thread where the client lives (Main Thread).

    // Ensure we have a client on the main thread (plugin load creates one usually, but we lazy init here)
    if (!g_client) {
        // If we are not on main thread, we can't create it with correct affinity easily without context.
        // But QMetaObject::invokeMethod helps.

        // Assumption: This function is called from a QObject's signal/slot or we can access the main thread context.
        // Actually, let's look at how it's called.
        // It is called from AudioCapture::processAudio (or buffer flush) via a direct call or lambda.
        // AudioCapture lives in a thread? No, AudioCapture is a QObject.
        // The audioCallback is a C callback on an audio thread.
        // AudioCapture::processAudio runs on that audio thread.
        // We need to move data to the main thread.

        // Better approach: AudioCapture emits a signal, connected to a slot on the main thread.
        // We will refactor this in audio-capture.cpp to Connect to a slot.
        // But to keep this global function working:

        // We can't safely access g_client here if it lives on another thread.
        // But if we create a temporary client here?
        // No, we want to reuse the connection manager.

        // We will assume this function is called on the Main Thread.
        // AudioCapture needs to emit a signal, and the slot that receives it calls this function.
        // OR AudioCapture emits 'audioPacketReady', and we connect that to a slot on the Main Thread Plugin instance
        // which calls this.
    }
}

// NOTE: We are removing the implementation of SendAudioToGemini here because
// we want the AudioCapture to emit a signal that is connected to a GeminiClient slot.
// BUT, to satisfy the linker for existing calls:

void SendAudioToGemini(const std::vector<int16_t>& pcmData, int sampleRate) {
    // This function acts as a bridge.
    // Ideally, we shouldn't use it if we rewire the signal/slot.
    // But let's make it thread-safe invoke the global client.

    if (!g_client) {
        // Create on main thread if possible, or just leak one for now if we are desperate.
        // Correct way: The Plugin class should own the GeminiClient.
        // We will rely on the "StartAudioCapture" connecting the signal correctly.
        // But the previous "StartAudioCapture" used a lambda that called this.
        // We will change that lambda to find the global client or emit a signal.
    }
}

// Actually, we will expose a helper to GET the client.
GeminiClient* GetGlobalGeminiClient() {
    if (!g_client) {
        // Assuming we are on main thread when this is first called (plugin load)
        g_client = new GeminiClient();
    }
    return g_client;
}
