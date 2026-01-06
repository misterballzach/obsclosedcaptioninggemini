#include "plugin-main.h"
#include "audio-capture.h"
#include "gemini-client.h"
#include <obs.h>
#include <obs-module.h>
#include <util/config-file.h>
#include <util/platform.h>
#include <QAction>
#include <QMainWindow>
#include <filesystem>
#include <mutex>
#include <QDateTime>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-gemini-captions", "en-US")

MODULE_EXPORT const char *obs_module_description(void)
{
    return "Closed Captioning using Gemini API";
}

static GeminiCaptionsDialog *settingsDialog = nullptr;
static CaptionDock *captionDock = nullptr;
static bool captioningActive = false;
static std::string g_apiKey;
static std::string g_audioSource;
static std::string g_textSource;
static std::mutex g_settingsMutex;

// CaptionDock Implementation
CaptionDock::CaptionDock(QWidget *parent) : QDockWidget(parent)
{
    setWindowTitle(obs_module_text("GeminiCaptions"));
    setObjectName("GeminiCaptionsDock"); // Unique name for saving layout state

    textDisplay = new QTextEdit(this);
    textDisplay->setReadOnly(true);
    textDisplay->setPlaceholderText("Captions will appear here...");

    setWidget(textDisplay);
}

void CaptionDock::AppendText(const QString &text)
{
    // Ensure we are on UI thread
    if (QThread::currentThread() != this->thread()) {
        QMetaObject::invokeMethod(this, [this, text]() {
            AppendText(text);
        });
        return;
    }

    QString timestamp = QDateTime::currentDateTime().toString("HH:mm:ss");
    textDisplay->append(QString("[%1] %2").arg(timestamp, text));
    // Scroll to bottom
    textDisplay->moveCursor(QTextCursor::End);
}

void AppendTextToDock(const std::string& text)
{
    if (captionDock) {
        captionDock->AppendText(QString::fromStdString(text));
    }
}

// GeminiCaptionsDialog Implementation (Existing)
GeminiCaptionsDialog::GeminiCaptionsDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(obs_module_text("GeminiCaptions"));
    setMinimumWidth(400);

    QVBoxLayout *layout = new QVBoxLayout(this);

    layout->addWidget(new QLabel(obs_module_text("ApiKey")));
    apiKeyEdit = new QLineEdit(this);
    apiKeyEdit->setEchoMode(QLineEdit::Password);
    layout->addWidget(apiKeyEdit);

    layout->addWidget(new QLabel(obs_module_text("AudioSource")));
    audioSourceCombo = new QComboBox(this);
    layout->addWidget(audioSourceCombo);

    layout->addWidget(new QLabel(obs_module_text("TextSource")));
    textSourceCombo = new QComboBox(this);
    layout->addWidget(textSourceCombo);

    startStopButton = new QPushButton(obs_module_text("StartCaptioning"), this);
    connect(startStopButton, &QPushButton::clicked, this, &GeminiCaptionsDialog::onToggleStartStop);
    layout->addWidget(startStopButton);

    QPushButton *saveButton = new QPushButton(obs_module_text("SaveSettings"), this);
    connect(saveButton, &QPushButton::clicked, this, &GeminiCaptionsDialog::onSave);
    layout->addWidget(saveButton);

    populateSources();
    loadSettings();

    if (captioningActive) {
        startStopButton->setText(obs_module_text("StopCaptioning"));
    }
}

GeminiCaptionsDialog::~GeminiCaptionsDialog() {}

void GeminiCaptionsDialog::populateSources()
{
    audioSourceCombo->clear();
    textSourceCombo->clear();

    // Enumerate sources
    struct obs_source_enum_data {
        QComboBox *audio;
        QComboBox *text;
    } data = {audioSourceCombo, textSourceCombo};

    obs_enum_sources([](void *data, obs_source_t *source) {
        auto *d = (obs_source_enum_data*)data;
        const char *name = obs_source_get_name(source);
        uint32_t flags = obs_source_get_output_flags(source);
        const char *id = obs_source_get_id(source);

        if (flags & OBS_SOURCE_AUDIO) {
            d->audio->addItem(name);
        }

        // Check for text sources (text_gdiplus or text_ft2_source)
        if (strcmp(id, "text_gdiplus") == 0 || strcmp(id, "text_ft2_source") == 0) {
            d->text->addItem(name);
        }

        return true;
    }, &data);
}

void GeminiCaptionsDialog::loadSettings()
{
    char *config_path = obs_module_get_config_path(obs_current_module(), "settings.json");
    if (!config_path) return;

    obs_data_t *settings = obs_data_create_from_json_file(config_path);
    bfree(config_path);

    if (!settings) return;

    const char *key = obs_data_get_string(settings, "api_key");
    const char *audio = obs_data_get_string(settings, "audio_source");
    const char *text = obs_data_get_string(settings, "text_source");

    apiKeyEdit->setText(key);

    int audioIdx = audioSourceCombo->findText(audio);
    if (audioIdx >= 0) audioSourceCombo->setCurrentIndex(audioIdx);

    int textIdx = textSourceCombo->findText(text);
    if (textIdx >= 0) textSourceCombo->setCurrentIndex(textIdx);

    std::lock_guard<std::mutex> lock(g_settingsMutex);
    g_apiKey = key;
    g_audioSource = audio;
    g_textSource = text;

    obs_data_release(settings);
}

void GeminiCaptionsDialog::saveSettings()
{
    char *config_path = obs_module_get_config_path(obs_current_module(), "settings.json");
    if (!config_path) return;

    obs_data_t *settings = obs_data_create();

    obs_data_set_string(settings, "api_key", apiKeyEdit->text().toUtf8().constData());
    obs_data_set_string(settings, "audio_source", audioSourceCombo->currentText().toUtf8().constData());
    obs_data_set_string(settings, "text_source", textSourceCombo->currentText().toUtf8().constData());

    obs_data_save_json_safe(settings, config_path, "tmp", "bak");

    {
        std::lock_guard<std::mutex> lock(g_settingsMutex);
        g_apiKey = apiKeyEdit->text().toStdString();
        g_audioSource = audioSourceCombo->currentText().toStdString();
        g_textSource = textSourceCombo->currentText().toStdString();
    }

    obs_data_release(settings);
    bfree(config_path);
}

void GeminiCaptionsDialog::onSave()
{
    saveSettings();
    accept();
}

void GeminiCaptionsDialog::onToggleStartStop()
{
    if (captioningActive) {
        StopCaptioning();
        startStopButton->setText(obs_module_text("StartCaptioning"));
    } else {
        saveSettings(); // Ensure latest settings are used
        StartCaptioning();
        startStopButton->setText(obs_module_text("StopCaptioning"));
    }
}

static void ShowConfig()
{
    QMainWindow *main = (QMainWindow*)obs_frontend_get_main_window();
    if (settingsDialog) {
        settingsDialog->raise();
        settingsDialog->activateWindow();
    } else {
        settingsDialog = new GeminiCaptionsDialog(main);
        settingsDialog->show();
    }
}

bool obs_module_load(void)
{
    obs_frontend_add_tools_menu_item("Gemini Captions", ShowConfig);

    // Create Dock
    QMainWindow *main = (QMainWindow*)obs_frontend_get_main_window();
    captionDock = new CaptionDock(main);
    obs_frontend_add_dock(captionDock);

    // Load initial settings
    char *config_path = obs_module_get_config_path(obs_current_module(), "settings.json");
    if (config_path) {
        obs_data_t *settings = obs_data_create_from_json_file(config_path);
        if (settings) {
            std::lock_guard<std::mutex> lock(g_settingsMutex);
            g_apiKey = obs_data_get_string(settings, "api_key");
            g_audioSource = obs_data_get_string(settings, "audio_source");
            g_textSource = obs_data_get_string(settings, "text_source");
            obs_data_release(settings);
        }
        bfree(config_path);
    }

    return true;
}

void obs_module_unload(void)
{
    StopCaptioning();
}

std::string GetGeminiAPIKey() {
    std::lock_guard<std::mutex> lock(g_settingsMutex);
    return g_apiKey;
}

std::string GetAudioSourceName() {
    std::lock_guard<std::mutex> lock(g_settingsMutex);
    return g_audioSource;
}

std::string GetTextSourceName() {
    std::lock_guard<std::mutex> lock(g_settingsMutex);
    return g_textSource;
}

void StartCaptioning()
{
    if (captioningActive) return;

    std::string key, source;
    {
        std::lock_guard<std::mutex> lock(g_settingsMutex);
        key = g_apiKey;
        source = g_audioSource;
    }

    if (key.empty() || source.empty()) {
        blog(LOG_WARNING, "Cannot start captioning: API Key or Audio Source missing.");
        return;
    }

    blog(LOG_INFO, "Starting Gemini Captioning...");

    if (StartAudioCapture(source)) {
        captioningActive = true;
    } else {
        blog(LOG_ERROR, "Failed to start audio capture.");
    }
}

void StopCaptioning()
{
    if (!captioningActive) return;

    blog(LOG_INFO, "Stopping Gemini Captioning...");
    StopAudioCapture();
    captioningActive = false;
}

bool IsCaptioningActive()
{
    return captioningActive;
}
