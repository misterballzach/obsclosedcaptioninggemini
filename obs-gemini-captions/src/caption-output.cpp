#include "caption-output.h"
#include "plugin-main.h"
#include <obs.h>
#include <obs-frontend-api.h>

void UpdateCaption(const std::string& text)
{
    // 1. Update Text Source (Open Captions)
    std::string textSourceName = GetTextSourceName();
    if (!textSourceName.empty()) {
        obs_source_t *source = obs_get_source_by_name(textSourceName.c_str());
        if (source) {
            obs_data_t *settings = obs_source_get_settings(source);
            obs_data_set_string(settings, "text", text.c_str());
            obs_source_update(source, settings);
            obs_data_release(settings);
            obs_source_release(source);
        }
    }

    // 2. Send Closed Captions (CEA-608) to Streaming Output
    // This is what Twitch uses for integrated captions.
    obs_output_t *output = obs_frontend_get_streaming_output();
    if (output) {
        // Use a reasonable display duration, e.g., 3.0 seconds.
        obs_output_output_caption_text1(output, text.c_str(), 3.0);
        obs_output_release(output);
    }

    // 3. Update UI Dock
    AppendTextToDock(text);

    // Also try recording output if active?
    obs_output_t *rec_output = obs_frontend_get_recording_output();
    if (rec_output) {
         obs_output_output_caption_text1(rec_output, text.c_str(), 3.0);
         obs_output_release(rec_output);
    }
}
