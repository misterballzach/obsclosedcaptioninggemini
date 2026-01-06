#pragma once

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <QDialog>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QLabel>
#include <string>
#include <QDockWidget>
#include <QTextEdit>

class GeminiCaptionsDialog : public QDialog {
    Q_OBJECT

public:
    GeminiCaptionsDialog(QWidget *parent = nullptr);
    ~GeminiCaptionsDialog();

    void loadSettings();
    void saveSettings();

private slots:
    void onSave();
    void onToggleStartStop();

private:
    QLineEdit *apiKeyEdit;
    QComboBox *audioSourceCombo;
    QComboBox *textSourceCombo;

    // Twitch Settings
    QLineEdit *twitchUserEdit;
    QLineEdit *twitchTokenEdit;
    QLineEdit *twitchChannelEdit;

    QPushButton *startStopButton;

    void populateSources();
};

class CaptionDock : public QDockWidget {
    Q_OBJECT
public:
    CaptionDock(QWidget *parent = nullptr);
    void AppendText(const QString &text);

private:
    QTextEdit *textDisplay;
};

void StartCaptioning();
void StopCaptioning();
bool IsCaptioningActive();
std::string GetGeminiAPIKey();
std::string GetAudioSourceName();
std::string GetTextSourceName();
std::string GetTwitchUser();
std::string GetTwitchToken();
std::string GetTwitchChannel();
void AppendTextToDock(const std::string& text);
