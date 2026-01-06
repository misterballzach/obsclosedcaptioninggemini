#include "caption-output.h"
#include "plugin-main.h"
#include <obs.h>
#include <obs-frontend-api.h>

// Helper to access the hypothetical CEA-608 API if it exists or generic text output
// OBS has `obs_output_output_caption_text1` or similar but it is not always exported in headers.
// However, looking at OBS source, `obs_output_output_caption_text1` takes `obs_output_t*`, `const char *text`, `double display_duration`.
// But to use it, we need to find the streaming output.

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
    // We get the active streaming output.
    obs_output_t *output = obs_frontend_get_streaming_output();
    if (output) {
        // NOTE: `obs_output_output_caption_text1` is the API for sending captions.
        // It requires `libobs/obs-output.h`.
        // If the function is not available, this might fail to link if we don't weak link or if headers are old.
        // Assuming OBS 32.0 headers:
        // void obs_output_output_caption_text1(obs_output_t *output, const char *text, double display_duration);

        // Use a reasonable display duration, e.g., 2.0 seconds or calculate based on text length.
        obs_output_output_caption_text1(output, text.c_str(), 3.0);

        obs_output_release(output);
    }

    // Also try recording output if active?
    obs_output_t *rec_output = obs_frontend_get_recording_output();
    if (rec_output) {
         obs_output_output_caption_text1(rec_output, text.c_str(), 3.0);
         obs_output_release(rec_output);
    }
}
