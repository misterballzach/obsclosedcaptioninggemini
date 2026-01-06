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
    QPushButton *startStopButton;

    void populateSources();
};

void StartCaptioning();
void StopCaptioning();
bool IsCaptioningActive();
std::string GetGeminiAPIKey();
std::string GetAudioSourceName();
std::string GetTextSourceName();
