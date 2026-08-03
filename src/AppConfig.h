#pragma once
#include <QString>
#include <QKeySequence>
#include <QApplication>

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
    QString vadPath = QApplication::applicationDirPath() + "/sherpa/vad/silero_vad.onnx";
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

    std::atomic<bool> continuousMode { false };
    std::atomic<bool> autoStopEnabled { true };

    QString hotkey;
    QString style;
    QString Language;

    AppConfig() = default;
    AppConfig(const AppConfig& other) { *this = other; }

    AppConfig& operator=(const AppConfig& other) {
        if (this == &other) return *this;
        backend = other.backend;
        audio = other.audio;
        sherpa = other.sherpa;
        gemini = other.gemini;
        terms = other.terms;
        gladiaKey = other.gladiaKey;
        replaceRules = other.replaceRules;
        continuousMode.store(other.continuousMode.load());
        autoStopEnabled.store(other.autoStopEnabled.load());
        hotkey = other.hotkey;
        style = other.style;
        Language = other.Language;
        return *this;
    }

};