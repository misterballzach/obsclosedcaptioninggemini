#pragma once

#include <vector>
#include <cstdint>
#include <string>
#include <functional>

void SendAudioToGemini(const std::vector<int16_t>& pcmData, int sampleRate);
void SendTextQueryToGemini(const std::string& prompt, std::function<void(std::string)> callback);
