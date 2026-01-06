#pragma once

#include <obs.h>
#include <string>

// Global settings accessors used by other modules
std::string GetGeminiAPIKey();
std::string GetAudioSourceName();
std::string GetTextSourceName();
std::string GetTwitchUser();
std::string GetTwitchToken();
std::string GetTwitchChannel();

// Global Control
void StartCaptioning();
void StopCaptioning();
bool IsCaptioningActive();

// UI Helpers
void AppendTextToDock(const std::string& text);

class GeminiCaptionsDialog;
class CaptionDock;
