#pragma once
#include <QMap>
#include <QString>
#include <QKeySequence>
#include <QApplication>
#include "utils/AppPaths.h"
#include "translation/LanguageRouting.h"

// macOS 上 Qt 把 Ctrl 映射为 ⌘，Ctrl+Alt+Y 会与应用级快捷键冲突；⌃⌥Y 不占用系统绑定
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

inline constexpr int kLocalTranslationEngine = 3;

struct PolishConfig {
    int aiEngineIndex = 0;  // 0 = Gemini, 1 = OpenAI兼容(本地/自訂), 2 = Ollama(本地), 3 = 本地离线翻译
    bool enableAiEnhancement = false;
    QString apiKey;
    QString apiUrl;
    QString projectId;
    QString model;
    // 各引擎各记一份润色模型：Gemini 的模型名在 Ollama 里不存在；model 始终是当前引擎的
    QMap<int, QString> modelByEngine;
    QMap<int, QString> apiUrlByEngine;  
    QMap<int, QString> apiKeyByEngine;
    QMap<int, QString> translateModelByEngine;
    QString prompt;
    QString vocab;
    QString aiStyle;
    QString targetLang;
    bool translateEnabled = false;
    QString translateSourceLang = "auto"; 
    // 翻译用的模型；空 = 跟随润色模型。只对能选模型的引擎（OpenAI 兼容 / Ollama）生效
    QString translateModel;

    QString activeTargetLang() const {
        return translateEnabled ? targetLang : QStringLiteral("不翻译");
    }
    bool usesLocalTranslation() const { return aiEngineIndex == kLocalTranslationEngine; }
    bool canChooseTranslateModel() const { return aiEngineIndex == 1 || aiEngineIndex == 2; }
    // 实际用于翻译的模型：Gemini 等没有单独的翻译模型，始终和润色同一个
    QString effectiveTranslateModel() const {
        return canChooseTranslateModel() && !translateModel.trimmed().isEmpty() ? translateModel.trimmed() : model;
    }
    // 云端引擎缺必填项时请求注定失败：Gemini 要 Key，OpenAI 兼容要地址；Ollama 有默认地址
    bool cloudEngineConfigured() const {
        if (aiEngineIndex == 0) return !apiKey.trimmed().isEmpty();
        if (aiEngineIndex == 1) return !apiUrl.trimmed().isEmpty();
        return true;
    }
    QString cloudTargetLang() const {
        return usesLocalTranslation() ? QStringLiteral("不翻译") : promptNameForStored(activeTargetLang());
    }
};

struct TermsConfig {
    QString path;
};

struct AppConfig {
    AsrBackendKind backend = AsrBackendKind::Sherpa;
    AudioConfig audio;
    
    SherpaConfig sherpa;
    QString gladiaKey;
    QString groqKey;
    QString geminiKey;

    PolishConfig polish;

    TermsConfig terms;
    QString replaceRules;

    std::atomic<bool> continuousMode { false };
    std::atomic<bool> autoStopEnabled { true };
    std::atomic<bool> liveCaptionEnabled { true };
    int liveCaptionFontSize = 12;  
    bool liveCaptionAtTop = false;
    // 听写结果是否同时输入到前台窗口；关掉后结果只进"实时转录"稿子。默认开
    bool injectToForeground = true;
    bool barAlwaysOnTop = true;
    // 上次停留的形态：false = 设置窗口（首次安装默认），true = 横条。见 ADR 0005
    bool barMode = false;

    QString hotkey;
    bool hotkeyHold = false;    

    QString style;
    QString Language;
    bool skipUpdateReminder = false;  

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
        geminiKey = other.geminiKey;
        replaceRules = other.replaceRules;
        continuousMode.store(other.continuousMode.load());
        autoStopEnabled.store(other.autoStopEnabled.load());
        liveCaptionEnabled.store(other.liveCaptionEnabled.load());
        liveCaptionFontSize = other.liveCaptionFontSize;
        liveCaptionAtTop = other.liveCaptionAtTop;
        injectToForeground = other.injectToForeground;
        barAlwaysOnTop = other.barAlwaysOnTop;
        barMode = other.barMode;
        hotkey = other.hotkey;
        hotkeyHold = other.hotkeyHold;
        style = other.style;
        Language = other.Language;
        skipUpdateReminder = other.skipUpdateReminder;
        return *this;
    }

};