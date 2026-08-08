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

    m_config.audio.deviceId = AudioConfig::kInvalidDeviceId;
    m_config.audio.deviceName = AudioConfig::kDefaultDeviceName;
    m_config.audio.outputDeviceName.clear();
    m_config.audio.sampleRate = 16000;
    m_config.audio.channels = 1;
    m_config.audio.bitsPerSample = 16;
    m_config.audio.voiceThreshold = 500;
    m_config.audio.silenceTimeoutMs = 800;
    m_config.audio.minRecordMs = 300;
    m_config.audio.maxRecordMs = 60000;

    m_config.sherpa.threads = 4;
    m_config.sherpa.useGpu = false;

    m_config.polish.enableAiEnhancement = false;
    m_config.polish.aiEngineIndex = 0;
    m_config.polish.aiStyle = "智慧预设";
    m_config.polish.targetLang = "不翻译";

    m_config.hotkey = "Ctrl+Alt+Y";
    m_config.continuousMode = false;

}

bool ConfigManager::load() {
    QSettings s(m_configPath, QSettings::IniFormat);

    m_config.hotkey = s.value("hotkey", "").toString();
    m_config.backend = static_cast<AsrBackendKind>(s.value("backend", 1).toInt());
    m_config.gladiaKey = s.value("gladiaKey").toString();
    m_config.groqKey = s.value("groqKey").toString();
    m_config.replaceRules = s.value("replaceRules").toString();
    m_config.continuousMode = s.value("continuousMode", false).toBool();
    m_config.autoStopEnabled = s.value("autoStopEnabled", true).toBool();

    s.beginGroup("audio");
    m_config.audio.deviceId = s.value("deviceId", -1).toInt();
    m_config.audio.deviceName = s.value("deviceName").toString();
    m_config.audio.outputDeviceName = s.value("outputDeviceName").toString();
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
    m_config.polish.aiEngineIndex = s.value("aiEngineIndex", 0).toInt();
    m_config.polish.enableAiEnhancement = s.value("enableAiEnhancement").toBool();
    m_config.polish.apiKey = s.value("apiKey").toString();
    m_config.polish.apiUrl = s.value("apiUrl").toString();
    m_config.polish.projectId = s.value("projectId").toString();
    m_config.polish.model = s.value("model").toString();
    m_config.polish.prompt = s.value("prompt").toString();
    m_config.polish.vocab = s.value("vocab").toString();
    m_config.polish.targetLang = s.value("targetLang").toString();
    m_config.polish.aiStyle = s.value("aiStyle").toString();
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
    s.setValue("continuousMode", m_config.continuousMode.load());
    s.setValue("autoStopEnabled", m_config.autoStopEnabled.load());

    s.beginGroup("audio");
    s.setValue("deviceId", m_config.audio.deviceId);
    s.setValue("deviceName", m_config.audio.deviceName);
    s.setValue("outputDeviceName", m_config.audio.outputDeviceName);
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
    s.setValue("enableAiEnhancement", m_config.polish.enableAiEnhancement);
    s.setValue("aiEngineIndex", m_config.polish.aiEngineIndex);
    s.setValue("apiKey", m_config.polish.apiKey);
    s.setValue("apiUrl", m_config.polish.apiUrl);
    s.setValue("projectId", m_config.polish.projectId);
    s.setValue("model", m_config.polish.model);
    s.setValue("prompt", m_config.polish.prompt);
    s.setValue("vocab", m_config.polish.vocab);
    s.setValue("aiStyle", m_config.polish.aiStyle);
    s.setValue("targetLang", m_config.polish.targetLang);
    s.endGroup();

    s.beginGroup("terms");
    s.setValue("path", m_config.terms.path);
    s.endGroup();

    s.sync();
    emit configSaved();
    return s.status() == QSettings::NoError;
}