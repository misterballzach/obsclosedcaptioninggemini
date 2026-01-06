#include "gemini-client.h"
#include "plugin-main.h"
#include "caption-output.h"
#include <curl/curl.h>
#include <vector>
#include <string>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <obs.h>
#include <thread>

// Simple Base64 encoder
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

// Write callback for libcurl
size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

// Improved JSON parser using obs_data_t
std::string ExtractTextFromJson(const std::string& json) {
    obs_data_t *data = obs_data_create_from_json(json.c_str());
    if (!data) return "";

    std::string result = "";

    obs_data_array_t *candidates = obs_data_get_array(data, "candidates");
    if (candidates) {
        size_t count = obs_data_array_count(candidates);
        if (count > 0) {
            obs_data_t *candidate = obs_data_array_item(candidates, 0);
            obs_data_t *content = obs_data_get_obj(candidate, "content");
            if (content) {
                obs_data_array_t *parts = obs_data_get_array(content, "parts");
                if (parts) {
                    size_t pcount = obs_data_array_count(parts);
                    if (pcount > 0) {
                        obs_data_t *part = obs_data_array_item(parts, 0);
                        const char *text = obs_data_get_string(part, "text");
                        if (text) result = text;
                        obs_data_release(part);
                    }
                    obs_data_array_release(parts);
                }
                obs_data_release(content);
            }
            obs_data_release(candidate);
        }
        obs_data_array_release(candidates);
    }

    obs_data_release(data);
    return result;
}

void PerformGeminiRequest(const std::string& jsonPayload, std::function<void(std::string)> callback = nullptr)
{
    std::string apiKey = GetGeminiAPIKey();
    if (apiKey.empty()) return;

    CURL *curl;
    CURLcode res;
    std::string readBuffer;

    curl = curl_easy_init();
    if(curl) {
        std::string url = "https://generativelanguage.googleapis.com/v1beta/models/gemini-1.5-flash:generateContent?key=" + apiKey;

        struct curl_slist *headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonPayload.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);

        // Timeout to prevent hanging
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L); // Increased timeout for text generation

        res = curl_easy_perform(curl);

        if(res != CURLE_OK) {
            blog(LOG_ERROR, "Gemini API request failed: %s", curl_easy_strerror(res));
        } else {
            std::string text = ExtractTextFromJson(readBuffer);
            if (!text.empty()) {
                if (callback) {
                    callback(text);
                } else {
                    blog(LOG_INFO, "Transcribed: %s", text.c_str());
                    UpdateCaption(text);
                }
            }
        }

        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }
}

void SendAudioToGemini(const std::vector<int16_t>& pcmData, int sampleRate)
{
    // Build WAV in memory
    std::vector<uint8_t> wavData;

    uint32_t totalDataLen = pcmData.size() * 2;
    uint32_t riffChunkSize = 36 + totalDataLen;
    uint32_t byteRate = sampleRate * 1 * 2;

    // RIFF
    wavData.insert(wavData.end(), {'R', 'I', 'F', 'F'});
    wavData.insert(wavData.end(), (uint8_t*)&riffChunkSize, (uint8_t*)&riffChunkSize + 4);
    wavData.insert(wavData.end(), {'W', 'A', 'V', 'E'});

    // fmt
    wavData.insert(wavData.end(), {'f', 'm', 't', ' '});
    uint32_t fmtChunkSize = 16;
    wavData.insert(wavData.end(), (uint8_t*)&fmtChunkSize, (uint8_t*)&fmtChunkSize + 4);
    uint16_t audioFormat = 1; // PCM
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

    // data
    wavData.insert(wavData.end(), {'d', 'a', 't', 'a'});
    wavData.insert(wavData.end(), (uint8_t*)&totalDataLen, (uint8_t*)&totalDataLen + 4);

    const uint8_t* pcmBytes = (const uint8_t*)pcmData.data();
    wavData.insert(wavData.end(), pcmBytes, pcmBytes + totalDataLen);

    std::string base64Audio = base64_encode(wavData.data(), wavData.size());

    obs_data_t *root = obs_data_create();
    obs_data_array_t *contentsArray = obs_data_array_create();
    obs_data_t *contentItem = obs_data_create();
    obs_data_array_t *partsArray = obs_data_array_create();

    obs_data_t *textPart = obs_data_create();
    obs_data_set_string(textPart, "text", "Transcribe the following audio to text. Output only the transcribed text, nothing else.");
    obs_data_array_push_back(partsArray, textPart);
    obs_data_release(textPart);

    obs_data_t *audioPart = obs_data_create();
    obs_data_t *inlineData = obs_data_create();
    obs_data_set_string(inlineData, "mimeType", "audio/wav");
    obs_data_set_string(inlineData, "data", base64Audio.c_str());
    obs_data_set_obj(audioPart, "inlineData", inlineData);
    obs_data_release(inlineData);
    obs_data_array_push_back(partsArray, audioPart);
    obs_data_release(audioPart);

    obs_data_set_array(contentItem, "parts", partsArray);
    obs_data_array_release(partsArray);
    obs_data_array_push_back(contentsArray, contentItem);
    obs_data_release(contentItem);
    obs_data_set_array(root, "contents", contentsArray);
    obs_data_array_release(contentsArray);

    const char *jsonOutput = obs_data_get_json(root);
    std::string jsonPayload(jsonOutput);
    obs_data_release(root);

    PerformGeminiRequest(jsonPayload, nullptr);
}

void SendTextQueryToGemini(const std::string& prompt, std::function<void(std::string)> callback)
{
    // Run in a thread to avoid blocking main thread (since this is called from TwitchBot on main thread)
    std::thread([prompt, callback]() {
        obs_data_t *root = obs_data_create();
        obs_data_array_t *contentsArray = obs_data_array_create();
        obs_data_t *contentItem = obs_data_create();
        obs_data_array_t *partsArray = obs_data_array_create();

        obs_data_t *textPart = obs_data_create();
        obs_data_set_string(textPart, "text", prompt.c_str());
        obs_data_array_push_back(partsArray, textPart);
        obs_data_release(textPart);

        obs_data_set_array(contentItem, "parts", partsArray);
        obs_data_array_release(partsArray);
        obs_data_array_push_back(contentsArray, contentItem);
        obs_data_release(contentItem);
        obs_data_set_array(root, "contents", contentsArray);
        obs_data_array_release(contentsArray);

        const char *jsonOutput = obs_data_get_json(root);
        std::string jsonPayload(jsonOutput);
        obs_data_release(root);

        PerformGeminiRequest(jsonPayload, callback);
    }).detach();
}
