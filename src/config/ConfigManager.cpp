#include "ConfigManager.h"
#include <QSettings>
#include <QFile>
#include "utils/Logger.h"
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QSaveFile>
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

bool ConfigManager::writeFileAtomically(const QString& path, const QByteArray& data, bool ownerOnly)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return false;
    if (ownerOnly) file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    if (file.write(data) != data.size()) {
        file.cancelWriting();
        return false;
    }
    return file.commit();
}

void ConfigManager::updateConfig(const AppConfig& newConfig) {
    QWriteLocker locker(&m_lock);
    m_config = newConfig;
}

void ConfigManager::applyDefaults() {
    QWriteLocker locker(&m_lock);
    m_config = AppConfig();
    
    m_config.backend = AsrBackendKind::Sherpa;
    m_config.audio.deviceId = AudioConfig::kInvalidDeviceId;
    m_config.audio.deviceName = AudioConfig::kDefaultDeviceName;
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
    m_config.polish.translateEnabled = false;
    m_config.polish.translateSourceLang = "auto";
    m_config.polish.translateModel.clear();
    m_config.polish.allowSelfSigned = true;

    m_config.hotkey = kDefaultHotkey;
    m_config.continuousMode = false;

}

bool ConfigManager::load() {
    QSettings s(m_configPath, QSettings::IniFormat);
    m_synced = true;

    // 缺键或存成空串都回到默认：空快捷键等于听写无法触发，且之后任何一次保存都会把空值固化下来
    m_config.hotkey = s.value("hotkey", kDefaultHotkey).toString();
    if (m_config.hotkey.isEmpty()) m_config.hotkey = kDefaultHotkey;
    m_config.hotkeyHold = s.value("hotkeyHold", false).toBool();
    m_config.backend = static_cast<AsrBackendKind>(s.value("backend").toInt());
    m_config.groqKey = s.value("groqKey").toString();
    m_config.replaceRules = s.value("replaceRules").toString();
    m_config.continuousMode = s.value("continuousMode", false).toBool();
    m_config.autoStopEnabled = s.value("autoStopEnabled", true).toBool();
    m_config.liveCaptionEnabled = s.value("liveCaptionEnabled", true).toBool();
    m_config.liveCaptionFontSize = s.value("liveCaptionFontSize", 12).toInt();
    m_config.liveCaptionAtTop = s.value("liveCaptionAtTop", false).toBool();
    m_config.injectToForeground = s.value("injectToForeground", true).toBool();
    m_config.barAlwaysOnTop = s.value("barAlwaysOnTop", true).toBool();
    m_config.barMode = s.value("barMode", false).toBool();
    m_config.geminiKey = s.value("geminiKey").toString();
    m_config.gladiaKey = s.value("gladiaKey").toString();
    m_config.groqKey = s.value("groqKey").toString();
    m_config.skipUpdateReminder = s.value("skipUpdateReminder", false).toBool();

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
    m_config.polish.aiEngineIndex = s.value("aiEngineIndex", 0).toInt();
    m_config.polish.enableAiEnhancement = s.value("enableAiEnhancement").toBool();
    m_config.polish.apiKey = s.value("apiKey").toString();
    m_config.polish.apiUrl = s.value("apiUrl").toString();
    m_config.polish.projectId = s.value("projectId").toString();
    m_config.polish.model = s.value("model").toString();
    for (int engine = 0; engine <= 2; ++engine) {
        const QString url = s.value(QStringLiteral("apiUrl_%1").arg(engine)).toString();
        const QString key = s.value(QStringLiteral("apiKey_%1").arg(engine)).toString();
        if (!url.isEmpty()) m_config.polish.apiUrlByEngine[engine] = url;
        if (!key.isEmpty()) m_config.polish.apiKeyByEngine[engine] = key;
    }
    // 旧版本只存了一份地址和密钥，归给当时选中的引擎
    if (!m_config.polish.apiUrlByEngine.contains(m_config.polish.aiEngineIndex))
        m_config.polish.apiUrlByEngine[m_config.polish.aiEngineIndex] = m_config.polish.apiUrl;
    if (!m_config.polish.apiKeyByEngine.contains(m_config.polish.aiEngineIndex))
        m_config.polish.apiKeyByEngine[m_config.polish.aiEngineIndex] = m_config.polish.apiKey;
    for (int engine = 0; engine <= 3; ++engine) {
        const QString saved = s.value(QStringLiteral("model_%1").arg(engine)).toString();
        if (!saved.isEmpty()) m_config.polish.modelByEngine[engine] = saved;
    }
    // 旧版本只存一个 model 归给当时引擎；Gemini 模型名不能归给 Ollama / OpenAI 兼容
    if (!m_config.polish.modelByEngine.contains(m_config.polish.aiEngineIndex)) {
        const bool foreignGemini = m_config.polish.aiEngineIndex != 0
                                   && m_config.polish.model.startsWith(QStringLiteral("gemini-"), Qt::CaseInsensitive);
        m_config.polish.modelByEngine[m_config.polish.aiEngineIndex] = foreignGemini ? QString() : m_config.polish.model;
        if (foreignGemini) m_config.polish.model.clear();
    }
    m_config.polish.prompt = s.value("prompt").toString();
    m_config.polish.vocab = s.value("vocab").toString();
    m_config.polish.targetLang = s.value("targetLang").toString();
    m_config.polish.aiStyle = s.value("aiStyle").toString();
    // 老配置没有这个开关：已经选过目标语言的用户升级后保持翻译开启
    const QString target = m_config.polish.targetLang;
    m_config.polish.translateEnabled = s.contains("translateEnabled")
        ? s.value("translateEnabled").toBool()
        : (!target.isEmpty() && target != "不翻译");
    m_config.polish.translateSourceLang = s.value("translateSourceLang", "auto").toString();
    m_config.polish.translateModel = s.value("translateModel", "").toString();
    m_config.polish.allowSelfSigned = s.value("allowSelfSigned", true).toBool();
    for (int engine = 1; engine <= 2; ++engine) {
        const QString saved = s.value(QStringLiteral("translateModel_%1").arg(engine)).toString();
        if (!saved.isEmpty()) m_config.polish.translateModelByEngine[engine] = saved;
    }
    if (!m_config.polish.translateModelByEngine.contains(m_config.polish.aiEngineIndex))
        m_config.polish.translateModelByEngine[m_config.polish.aiEngineIndex] = m_config.polish.translateModel;
    s.endGroup();

    s.beginGroup("terms");
    m_config.terms.path = s.value("path").toString();
    s.endGroup();

    emit configLoaded();
    return true;
}

bool ConfigManager::save() {
    // 还没载入过就保存，写出去的是未载入的默认值，会把用户已有的快捷键、密钥等整份覆盖。
    if (!m_synced && QFile::exists(m_configPath)) {
        LOG_WARN("Refused to save configuration before it was loaded (would overwrite the existing file with defaults)");
        return false;
    }
    m_synced = true;
    QSettings s(m_configPath, QSettings::IniFormat);

    s.setValue("hotkey", m_config.hotkey);
    s.setValue("hotkeyHold", m_config.hotkeyHold);
    s.setValue("backend", static_cast<int>(m_config.backend));
    s.setValue("replaceRules", m_config.replaceRules);
    s.setValue("continuousMode", m_config.continuousMode.load());
    s.setValue("autoStopEnabled", m_config.autoStopEnabled.load());
    s.setValue("liveCaptionEnabled", m_config.liveCaptionEnabled.load());
    s.setValue("liveCaptionFontSize", m_config.liveCaptionFontSize);
    s.setValue("liveCaptionAtTop", m_config.liveCaptionAtTop);
    s.setValue("injectToForeground", m_config.injectToForeground);
    s.setValue("barAlwaysOnTop", m_config.barAlwaysOnTop);
    s.setValue("barMode", m_config.barMode);
    s.setValue("geminiKey", m_config.geminiKey);
    s.setValue("gladiaKey", m_config.gladiaKey);
    s.setValue("groqKey", m_config.groqKey);
    s.setValue("skipUpdateReminder", m_config.skipUpdateReminder);

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
    s.setValue("enableAiEnhancement", m_config.polish.enableAiEnhancement);
    s.setValue("aiEngineIndex", m_config.polish.aiEngineIndex);
    s.setValue("apiKey", m_config.polish.apiKey);
    s.setValue("apiUrl", m_config.polish.apiUrl);
    s.setValue("projectId", m_config.polish.projectId);
    s.setValue("model", m_config.polish.model);
    m_config.polish.apiUrlByEngine[m_config.polish.aiEngineIndex] = m_config.polish.apiUrl;
    m_config.polish.apiKeyByEngine[m_config.polish.aiEngineIndex] = m_config.polish.apiKey;
    for (auto it = m_config.polish.apiUrlByEngine.cbegin(); it != m_config.polish.apiUrlByEngine.cend(); ++it)
        s.setValue(QStringLiteral("apiUrl_%1").arg(it.key()), it.value());
    for (auto it = m_config.polish.apiKeyByEngine.cbegin(); it != m_config.polish.apiKeyByEngine.cend(); ++it)
        s.setValue(QStringLiteral("apiKey_%1").arg(it.key()), it.value());
    m_config.polish.modelByEngine[m_config.polish.aiEngineIndex] = m_config.polish.model;
    for (auto it = m_config.polish.modelByEngine.cbegin(); it != m_config.polish.modelByEngine.cend(); ++it)
        s.setValue(QStringLiteral("model_%1").arg(it.key()), it.value());
    s.setValue("prompt", m_config.polish.prompt);
    s.setValue("vocab", m_config.polish.vocab);
    s.setValue("aiStyle", m_config.polish.aiStyle);
    s.setValue("targetLang", m_config.polish.targetLang);
    s.setValue("translateEnabled", m_config.polish.translateEnabled);
    s.setValue("translateSourceLang", m_config.polish.translateSourceLang);
    s.setValue("translateModel", m_config.polish.translateModel);
    s.setValue("allowSelfSigned", m_config.polish.allowSelfSigned);
    m_config.polish.translateModelByEngine[m_config.polish.aiEngineIndex] = m_config.polish.translateModel;
    for (auto it = m_config.polish.translateModelByEngine.cbegin(); it != m_config.polish.translateModelByEngine.cend(); ++it)
        s.setValue(QStringLiteral("translateModel_%1").arg(it.key()), it.value());
    s.endGroup();

    s.beginGroup("terms");
    s.setValue("path", m_config.terms.path);
    s.endGroup();

    s.sync();
    emit configSaved();
    return s.status() == QSettings::NoError;
}