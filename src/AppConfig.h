#pragma once
#include <QString>
#include <QKeySequence>

enum class AsrBackendKind {
    Sherpa = 0,
    Gemini = 1,
    Groq = 2,
    Gladia = 3,
};

struct AudioConfig {
    int deviceId = -1;
    QString deviceName;
    int sampleRate = 16000;
    int channels = 1;
    int bitsPerSample = 16;
    int voiceThreshold = 500;
    int silenceTimeoutMs = 800;
    int minRecordMs = 300;
    int maxRecordMs = 60000;
};

struct SherpaConfig {
    QString vadPath;
    bool useGpu = false;
    int threads = 4;
    QString languageModel;
    QString localModelRepoId;
    QString hotwords;
    float hotscores = 1.5;
};

struct GeminiConfig {
    QString apiKey;
    QString apiUrl;
    QString projectId;
    QString model;
    QString prompt;
    QString vocab;
    QString geminiStyle;
    QString targetLang;
    int aiEngineIndex = 0;
    bool enableGemini;
};

struct TermsConfig {
    QString path;
};

struct AppConfig {
    AsrBackendKind backend = AsrBackendKind::Sherpa;
    AudioConfig audio;
    SherpaConfig sherpa;
    GeminiConfig gemini;
    TermsConfig terms;

    QString gladiaKey;
    QString replaceRules;

    bool continuousMode = false;
    bool autoStopEnabled = true;

    QString hotkey;
    QString style;
    QString Language;

};