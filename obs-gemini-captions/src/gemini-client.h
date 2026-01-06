#pragma once

#include <vector>
#include <cstdint>

void SendAudioToGemini(const std::vector<int16_t>& pcmData, int sampleRate);
