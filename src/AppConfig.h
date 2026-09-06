#pragma once
#include <QString>
#include <QKeySequence>
#include <QApplication>
#include "utils/AppPaths.h"

// macOS 上 Qt 把 Ctrl 映射为 ⌘、Meta 映射为 ⌃，所以 Windows 的 Ctrl+Alt+Y 会变成
// ⌘⌥Y 并与应用级快捷键频繁冲突；⌃⌥Y 则不占用任何 macOS 系统绑定。
#ifdef Q_OS_MACOS
inline const QString kDefaultHotkey = QStringLiteral("Alt+Meta+Y");
#else
inline const QString kDefaultHotkey = QStringLiteral("Ctrl+Alt+Y");
#endif

enum class AsrBackendKind {
    Sherpa = 0,
    Gemini = 1,
    Groq = 2,
    Gladia = 3,
};

struct AudioConfig {
    static constexpr int kInvalidDeviceId = -1;
    inline static const QString kDefaultDeviceName = QStringLiteral("系统默认录音设备");

    int deviceId = kInvalidDeviceId;
    QString deviceName = kDefaultDeviceName;
    QString outputDeviceName;
    int sampleRate = 16000;
    int channels = 1;
    int bitsPerSample = 16;
    int voiceThreshold = 500;
    int silenceTimeoutMs = 800;
    int minRecordMs = 300;
    int maxRecordMs = 60000;
};

struct SherpaConfig {
    QString vadPath = AppPaths::vadModelFile();
    bool useGpu = false;
    int threads = 4;
    QString languageModel;
    QString localModelRepoId;
    QString hotwords;
    float hotscores = 1.5;
};

struct PolishConfig {
    int aiEngineIndex = 0;  // 0 = Gemini, 1 = OpenAI兼容(本地/自訂), 2 = Ollama(本地)
    bool enableAiEnhancement = false;
    QString apiKey;
    QString apiUrl;
    QString projectId;
    QString model;
    QString prompt;
    QString vocab;
    QString aiStyle;
    QString targetLang;
};

struct TermsConfig {
    QString path;
};

struct AppConfig {
    AsrBackendKind backend = AsrBackendKind::Sherpa;
    AudioConfig audio;
    
    // 转录
    SherpaConfig sherpa;
    QString gladiaKey;
    QString groqKey;
    QString geminiKey;

    // 润色和翻译
    PolishConfig polish;

    // 词典
    TermsConfig terms;
    QString replaceRules;

    std::atomic<bool> continuousMode { false };
    std::atomic<bool> autoStopEnabled { true };

    QString hotkey;

    QString style;
    QString Language;
    bool skipUpdateReminder = false;  // 用户勾选"不再提醒"后为真，停止更新弹窗

    AppConfig() = default;
    AppConfig(const AppConfig& other) { *this = other; }

    AppConfig& operator=(const AppConfig& other) {
        if (this == &other) return *this;
        backend = other.backend;
        audio = other.audio;
        sherpa = other.sherpa;
        polish = other.polish;
        terms = other.terms;
        gladiaKey = other.gladiaKey;
        groqKey = other.groqKey;
        replaceRules = other.replaceRules;
        continuousMode.store(other.continuousMode.load());
        autoStopEnabled.store(other.autoStopEnabled.load());
        hotkey = other.hotkey;
        style = other.style;
        Language = other.Language;
        skipUpdateReminder = other.skipUpdateReminder;
        return *this;
    }

};