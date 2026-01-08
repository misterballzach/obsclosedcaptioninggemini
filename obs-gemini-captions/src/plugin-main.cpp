#define OBS_FRONTEND_API_VERSION 1
#include <obs-frontend-api.h>
#include "plugin-main.h"
#include "audio-capture.h"
#include "gemini-client.h"
#include "twitch-bot.h"
#include <obs.h>
#include <obs-module.h>
#include <util/config-file.h>
#include <util/platform.h>
#include <QAction>
#include <QMainWindow>
#include <QDockWidget>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QDialog>
#include <filesystem>
#include <mutex>
#include <QDateTime>
#include <QThread>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-gemini-captions", "en-US")

MODULE_EXPORT const char *obs_module_description(void)
{
    return "Closed Captioning using Gemini API";
}

MODULE_EXPORT const char *obs_module_name(void)
{
    return "Gemini Captions";
}

MODULE_EXPORT const char *obs_module_author(void)
{
    return "Jules (AI)";
}

// Forward Declaration for local classes
class CaptionDock : public QDockWidget {
public:
    CaptionDock(QWidget *parent = nullptr);
    void AppendText(const QString &text);
private:
    QTextEdit *textDisplay;
};

class GeminiCaptionsDialog : public QDialog {
public:
    GeminiCaptionsDialog(QWidget *parent = nullptr);
    ~GeminiCaptionsDialog();
private slots:
    void onSave();
    void onToggleStartStop();
private:
    void populateSources();
    void loadSettings();
    void saveSettings();

    QLineEdit *apiKeyEdit;
    QComboBox *audioSourceCombo;
    QComboBox *textSourceCombo;

    QLineEdit *twitchUserEdit;
    QLineEdit *twitchTokenEdit;
    QLineEdit *twitchChannelEdit;

    QPushButton *startStopButton;
};

static GeminiCaptionsDialog *settingsDialog = nullptr;
static CaptionDock *captionDock = nullptr;
static TwitchBot *twitchBot = nullptr;
static bool captioningActive = false;

// Settings Globals
static std::string g_apiKey;
static std::string g_audioSource;
static std::string g_textSource;
static std::string g_twitchUser;
static std::string g_twitchToken;
static std::string g_twitchChannel;

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

// GeminiCaptionsDialog Implementation
GeminiCaptionsDialog::GeminiCaptionsDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(obs_module_text("GeminiCaptions"));
    setMinimumWidth(400);

    QVBoxLayout *layout = new QVBoxLayout(this);

    // Gemini Settings
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

    // Twitch Settings
    layout->addWidget(new QLabel("--- Twitch Integration ---"));

    layout->addWidget(new QLabel(obs_module_text("TwitchUser")));
    twitchUserEdit = new QLineEdit(this);
    layout->addWidget(twitchUserEdit);

    layout->addWidget(new QLabel(obs_module_text("TwitchToken")));
    twitchTokenEdit = new QLineEdit(this);
    twitchTokenEdit->setEchoMode(QLineEdit::Password);
    layout->addWidget(twitchTokenEdit);

    layout->addWidget(new QLabel(obs_module_text("TwitchChannel")));
    twitchChannelEdit = new QLineEdit(this);
    layout->addWidget(twitchChannelEdit);

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

    const char *twitchUser = obs_data_get_string(settings, "twitch_user");
    const char *twitchToken = obs_data_get_string(settings, "twitch_token");
    const char *twitchChannel = obs_data_get_string(settings, "twitch_channel");

    apiKeyEdit->setText(key);

    int audioIdx = audioSourceCombo->findText(audio);
    if (audioIdx >= 0) audioSourceCombo->setCurrentIndex(audioIdx);

    int textIdx = textSourceCombo->findText(text);
    if (textIdx >= 0) textSourceCombo->setCurrentIndex(textIdx);

    twitchUserEdit->setText(twitchUser);
    twitchTokenEdit->setText(twitchToken);
    twitchChannelEdit->setText(twitchChannel);

    std::lock_guard<std::mutex> lock(g_settingsMutex);
    g_apiKey = key;
    g_audioSource = audio;
    g_textSource = text;
    g_twitchUser = twitchUser;
    g_twitchToken = twitchToken;
    g_twitchChannel = twitchChannel;

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

    obs_data_set_string(settings, "twitch_user", twitchUserEdit->text().toUtf8().constData());
    obs_data_set_string(settings, "twitch_token", twitchTokenEdit->text().toUtf8().constData());
    obs_data_set_string(settings, "twitch_channel", twitchChannelEdit->text().toUtf8().constData());

    obs_data_save_json_safe(settings, config_path, "tmp", "bak");

    {
        std::lock_guard<std::mutex> lock(g_settingsMutex);
        g_apiKey = apiKeyEdit->text().toStdString();
        g_audioSource = audioSourceCombo->currentText().toStdString();
        g_textSource = textSourceCombo->currentText().toStdString();
        g_twitchUser = twitchUserEdit->text().toStdString();
        g_twitchToken = twitchTokenEdit->text().toStdString();
        g_twitchChannel = twitchChannelEdit->text().toStdString();
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

// Fixed signature for OBS 32.0.4 callback (takes void* data)
static void ShowConfig(void *data)
{
    Q_UNUSED(data);
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
    // Updated signature: name, callback, private_data
    obs_frontend_add_tools_menu_item("Gemini Captions", ShowConfig, nullptr);

    // Create Dock - Native Qt method for OBS 32+
    // Note: 'obs_frontend_add_dock_by_id' exists in some versions, but direct Qt parenting
    // is more robust against API changes in the frontend wrapper.
    QMainWindow *main = (QMainWindow*)obs_frontend_get_main_window();
    captionDock = new CaptionDock(main);

    // OBS usually puts docks in a specific area, Bottom is fine for captions
    main->addDockWidget(Qt::BottomDockWidgetArea, captionDock);

    // We should allow the user to toggle it.
    // Usually adding a dock widget to QMainWindow automatically adds it to the context menu of docks.
    // So explicit menu registration might not be strictly needed if native parenting works correctly.
    // We ensure it is shown by default.
    captionDock->show();

    // Initialize Twitch Bot (lived on main thread)
    twitchBot = new TwitchBot(main);

    // Load initial settings
    char *config_path = obs_module_get_config_path(obs_current_module(), "settings.json");
    if (config_path) {
        obs_data_t *settings = obs_data_create_from_json_file(config_path);
        if (settings) {
            std::lock_guard<std::mutex> lock(g_settingsMutex);
            g_apiKey = obs_data_get_string(settings, "api_key");
            g_audioSource = obs_data_get_string(settings, "audio_source");
            g_textSource = obs_data_get_string(settings, "text_source");
            g_twitchUser = obs_data_get_string(settings, "twitch_user");
            g_twitchToken = obs_data_get_string(settings, "twitch_token");
            g_twitchChannel = obs_data_get_string(settings, "twitch_channel");
            obs_data_release(settings);
        }
        bfree(config_path);
    }

    return true;
}

void obs_module_unload(void)
{
    StopCaptioning();
    if (twitchBot) {
        delete twitchBot;
        twitchBot = nullptr;
    }
    // Dock is owned by Main Window (parent), so it cleans up itself usually,
    // but we can be safe:
    if (captionDock) {
        // If we don't delete it, it might persist? OBS reloads plugins?
        // Usually safer to let Qt parent handle it or delete explicitly if we owned it.
        // Since we parented to main, main will delete it.
        // But if module unloads and main stays open (rare for OBS?), we should remove it.
        QMainWindow *main = (QMainWindow*)obs_frontend_get_main_window();
        if (main) main->removeDockWidget(captionDock);
        delete captionDock;
        captionDock = nullptr;
    }
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

std::string GetTwitchUser() {
    std::lock_guard<std::mutex> lock(g_settingsMutex);
    return g_twitchUser;
}

std::string GetTwitchToken() {
    std::lock_guard<std::mutex> lock(g_settingsMutex);
    return g_twitchToken;
}

std::string GetTwitchChannel() {
    std::lock_guard<std::mutex> lock(g_settingsMutex);
    return g_twitchChannel;
}

void StartCaptioning()
{
    if (captioningActive) return;

    std::string key, source, twUser, twToken, twChan;
    {
        std::lock_guard<std::mutex> lock(g_settingsMutex);
        key = g_apiKey;
        source = g_audioSource;
        twUser = g_twitchUser;
        twToken = g_twitchToken;
        twChan = g_twitchChannel;
    }

    if (key.empty()) {
        blog(LOG_WARNING, "Cannot start captioning: API Key missing.");
        return;
    }

    blog(LOG_INFO, "Starting Gemini Captioning...");

    if (!source.empty()) {
        if (StartAudioCapture(source)) {
            captioningActive = true;
        } else {
            blog(LOG_ERROR, "Failed to start audio capture.");
        }
    } else {
         captioningActive = true;
    }

    if (!twUser.empty() && !twToken.empty() && !twChan.empty()) {
        if (twitchBot) twitchBot->Connect();
    }
}

void StopCaptioning()
{
    if (!captioningActive) return;

    blog(LOG_INFO, "Stopping Gemini Captioning...");
    StopAudioCapture();
    if (twitchBot) twitchBot->Disconnect();
    captioningActive = false;
}

bool IsCaptioningActive()
{
    return captioningActive;
}
