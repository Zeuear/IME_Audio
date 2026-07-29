#include "ConfigManager.h"
#include <QSettings>
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QApplication>

ConfigManager& ConfigManager::instance()
{
    static ConfigManager _instance;
    return _instance;
}

ConfigManager::ConfigManager(QObject *parent) : QObject(parent) {}

void ConfigManager::setConfigFilePath(const QString& path)
{
    m_configPath = path;
}

QString ConfigManager::configFilePath() {
    return m_configPath;
}

void ConfigManager::updateConfig(const AppConfig& newConfig) {
    QWriteLocker locker(&m_lock);
    m_config = newConfig;
}

void ConfigManager::applyDefaults() {
    QWriteLocker locker(&m_lock);
    m_config = AppConfig();

    m_config.audio.deviceName = "系统默认录音设备";
    m_config.audio.sampleRate = 16000;
    m_config.audio.channels = 1;
    m_config.audio.bitsPerSample = 16;
    m_config.audio.voiceThreshold = 500;
    m_config.audio.silenceTimeoutMs = 800;
    m_config.audio.minRecordMs = 300;
    m_config.audio.maxRecordMs = 60000;

    m_config.sherpa.threads = 4;
    m_config.sherpa.useGpu = false;

    m_config.gemini.aiEngineIndex = 0;
    m_config.gemini.geminiStyle = "智慧预设";
    m_config.gemini.targetLang = "不翻译";

    m_config.hotkey = "Ctrl+Alt+Y";
    m_config.continuousMode = false;

}

bool ConfigManager::load() {
    QSettings s(m_configPath, QSettings::IniFormat);

    m_config.hotkey = s.value("hotkey", "").toString();
    m_config.backend = static_cast<AsrBackendKind>(s.value("backend", 1).toInt());
    m_config.gladiaKey = s.value("gladiaKey").toString();
    m_config.replaceRules = s.value("replaceRules").toString();
    m_config.continuousMode = s.value("continuousMode", false).toBool();
    m_config.autoStopEnabled = s.value("autoStopEnabled", true).toBool();

    s.beginGroup("audio");
    m_config.audio.deviceId = s.value("deviceId", -1).toInt();
    m_config.audio.deviceName = s.value("deviceName").toString();
    m_config.audio.sampleRate = s.value("sampleRate", 16000).toInt();
    m_config.audio.channels = s.value("channels", 1).toInt();
    m_config.audio.bitsPerSample = s.value("bitsPerSample", 16).toInt();
    m_config.audio.voiceThreshold = s.value("voiceThreshold", 500).toInt();
    m_config.audio.silenceTimeoutMs = s.value("silenceTimeoutMs", 800).toInt();
    m_config.audio.minRecordMs = s.value("minRecordMs", 300).toInt();
    m_config.audio.maxRecordMs = s.value("maxRecordMs", 60000).toInt();
    s.endGroup();

    s.beginGroup("sherpa");
    m_config.sherpa.useGpu = s.value("useGpu", false).toBool();
    m_config.sherpa.threads = s.value("threads", 4).toInt();
    m_config.sherpa.localModelRepoId = s.value("localModelRepoId", "").toString();
	m_config.sherpa.languageModel = s.value("languageModel", "").toString();
    s.endGroup();

    s.beginGroup("gemini");
    m_config.gemini.apiKey = s.value("apiKey").toString();
    m_config.gemini.apiUrl = s.value("apiUrl").toString();
    m_config.gemini.projectId = s.value("projectId").toString();
    m_config.gemini.model = s.value("model").toString();
    m_config.gemini.prompt = s.value("prompt").toString();
    m_config.gemini.vocab = s.value("vocab").toString();
    m_config.gemini.aiEngineIndex = s.value("aiEngineIndex", 0).toInt();
    m_config.gemini.targetLang = s.value("targetLang").toString();
    m_config.gemini.geminiStyle = s.value("geminiStyle").toString();
    s.endGroup();

    s.beginGroup("terms");
    m_config.terms.path = s.value("path").toString();
    s.endGroup();


    emit configLoaded();
    return true;
}

bool ConfigManager::save() {
    QSettings s(m_configPath, QSettings::IniFormat);

    s.setValue("hotkey", m_config.hotkey);
    s.setValue("backend", static_cast<int>(m_config.backend));
    s.setValue("gladiaKey", m_config.gladiaKey);
    s.setValue("replaceRules", m_config.replaceRules);
    s.setValue("continuousMode", m_config.continuousMode);
    s.setValue("autoStopEnabled", m_config.autoStopEnabled);

    s.beginGroup("audio");
    s.setValue("deviceId", m_config.audio.deviceId);
    s.setValue("deviceName", m_config.audio.deviceName);
    s.setValue("sampleRate", m_config.audio.sampleRate);
    s.setValue("channels", m_config.audio.channels);
    s.setValue("bitsPerSample", m_config.audio.bitsPerSample);
    s.setValue("voiceThreshold", m_config.audio.voiceThreshold);
    s.setValue("silenceTimeoutMs", m_config.audio.silenceTimeoutMs);
    s.setValue("minRecordMs", m_config.audio.minRecordMs);
    s.setValue("maxRecordMs", m_config.audio.maxRecordMs);
    s.endGroup();

    s.beginGroup("sherpa");
    s.setValue("useGpu", m_config.sherpa.useGpu);
    s.setValue("threads", m_config.sherpa.threads);
    s.setValue("languageModel", m_config.sherpa.languageModel);
    s.setValue("localModelRepoId", m_config.sherpa.localModelRepoId);
    s.endGroup();

    s.beginGroup("gemini");
    s.setValue("apiKey", m_config.gemini.apiKey);
    s.setValue("apiUrl", m_config.gemini.apiUrl);
    s.setValue("projectId", m_config.gemini.projectId);
    s.setValue("model", m_config.gemini.model);
    s.setValue("prompt", m_config.gemini.prompt);
    s.setValue("vocab", m_config.gemini.vocab);
    s.setValue("aiEngineIndex", m_config.gemini.aiEngineIndex);
    s.setValue("geminiStyle", m_config.gemini.geminiStyle);
    s.setValue("targetLang", m_config.gemini.targetLang);
    s.endGroup();

    s.beginGroup("terms");
    s.setValue("path", m_config.terms.path);
    s.endGroup();

    s.sync();
    emit configSaved();
    return s.status() == QSettings::NoError;
}