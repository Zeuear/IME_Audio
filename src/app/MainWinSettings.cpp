#include "MainWin.h"
#include "ui_MainWin.h"

#include <QTimer>
#include "config/ConfigManager.h"
#include "sherpa/SherpaManager.h"
#include "utils/Logger.h"
#include "ui/controls/SettingsBinder.h"
#include "ui/pages/FileTranscribeWidget.h"
#include "ui/controls/ControlBar.h"
#include "ui/controls/ExpandPanel.h"

namespace {
using Apply = SettingsBinder::Apply;
using Opt = SettingsBinder::Options;
constexpr auto kRecorder = SettingsBinder::Recorder;
constexpr auto kCaption = SettingsBinder::LiveCaption;
} 

void MainWin::registerSettings()
{
    AppConfig& cfg = ConfigManager::instance().config();
    m_settings = new SettingsBinder(cfg, this);
    SettingsBinder* s = m_settings;

    s->bindIndex(ui->identification_backend_comb,
        [](const AppConfig& c) { return int(c.backend); },
        [](AppConfig& c, int v) { c.backend = AsrBackendKind(v); },
        Opt{ Apply::Instant, 0, true });
        
    // 设备按名称记，插拔设备后序号会变；名称找不到时退回序号
    s->bindIndex(ui->recording_equ_comb,
        [this](const AppConfig& c) {
            const int i = ui->recording_equ_comb->findText(c.audio.deviceName);
            return i >= 0 ? i : c.audio.deviceId;
        },
        [this](AppConfig& c, int v) {
            c.audio.deviceId = v;
            c.audio.deviceName = ui->recording_equ_comb->itemText(v);
        },
        Opt{ Apply::Instant, kRecorder, true });
    s->bind(ui->grop_key_edit,
        [](const AppConfig& c) { return c.groqKey; },
        [](AppConfig& c, const QString& v) { c.groqKey = v; },
        Opt{ Apply::OnCommit });
    s->bind(ui->gladia_key_edit,
        [](const AppConfig& c) { return c.gladiaKey; },
        [](AppConfig& c, const QString& v) { c.gladiaKey = v; },
        Opt{ Apply::OnCommit });
    s->bind(ui->gemini_key_edit,
        [](const AppConfig& c) { return c.geminiKey; },
        [](AppConfig& c, const QString& v) { c.geminiKey = v; },
        Opt{ Apply::OnCommit });

    s->bind(ui->autiomatic_monitoring_checkbox,
        [](const AppConfig& c) { return c.continuousMode.load(); },
        [](AppConfig& c, bool v) { c.continuousMode = v; },
        Opt{ Apply::Instant, kRecorder, true });
    // 线程数在下次听写加载模型时生效（reloadModel 会比较配置）
    s->bind(ui->cpu_thread_number_spin,
        [](const AppConfig& c) { return c.sherpa.threads; },
        [](AppConfig& c, int v) { c.sherpa.threads = v; },
        Opt{ Apply::OnCommit });
    s->bind(ui->volume_threshold_spin,
        [](const AppConfig& c) { return c.audio.voiceThreshold; },
        [](AppConfig& c, int v) { c.audio.voiceThreshold = v; },
        Opt{ Apply::OnCommit, kRecorder });
    s->bind(ui->mute_duration_spin,
        [](const AppConfig& c) { return c.audio.silenceTimeoutMs; },
        [](AppConfig& c, int v) { c.audio.silenceTimeoutMs = v; },
        Opt{ Apply::OnCommit, kRecorder });
    s->bind(ui->shortest_recording_spin,
        [](const AppConfig& c) { return c.audio.minRecordMs; },
        [](AppConfig& c, int v) { c.audio.minRecordMs = v; },
        Opt{ Apply::OnCommit, kRecorder });
    s->bind(ui->longest_recording_spin,
        [](const AppConfig& c) { return c.audio.maxRecordMs; },
        [](AppConfig& c, int v) { c.audio.maxRecordMs = v; },
        Opt{ Apply::OnCommit, kRecorder });

    s->bind(ui->live_caption_checkbox,
        [](const AppConfig& c) { return c.liveCaptionEnabled.load(); },
        [](AppConfig& c, bool v) { c.liveCaptionEnabled = v; },
        Opt{ Apply::Instant, kCaption });
    s->bind(ui->live_caption_font_spin,
        [](const AppConfig& c) { return c.liveCaptionFontSize; },
        [](AppConfig& c, int v) { c.liveCaptionFontSize = v; },
        Opt{ Apply::OnCommit, kCaption });
    s->bindIndex(ui->live_caption_position_comb,
        [](const AppConfig& c) { return c.liveCaptionAtTop ? 1 : 0; },
        [](AppConfig& c, int v) { c.liveCaptionAtTop = v == 1; },
        Opt{ Apply::Instant, kCaption });

    s->bindIndex(ui->hotkey_mode_comb,
        [](const AppConfig& c) { return c.hotkeyHold ? 1 : 0; },
        [](AppConfig& c, int v) { c.hotkeyHold = v == 1; },
        Opt{ Apply::Instant, 0, true });
    s->bind(ui->inject_checkbox,
        [](const AppConfig& c) { return c.injectToForeground; },
        [](AppConfig& c, const bool& v) { c.injectToForeground = v; },
        Opt{ Apply::Instant, 0, false });
    s->addLockedWidget(ui->shortcut_edit);
    connect(ui->shortcut_edit, &ShortcutEdit::shortcutChanged, this, [this](const QString& hotkey) {
        // 只勾了修饰键、还没填按键时拼出来是空串，不能写进配置，否则重启后整组快捷键都丢了
        if (hotkey.isEmpty()) return;
        ConfigManager::instance().config().hotkey = hotkey;
        applySettings(0);
    });

    s->addLockedWidget(ui->language_comb);
    s->addLockedWidget(ui->model_list_widget);
    s->addLockedWidget(ui->uninstall_sherpa_btn);
    s->addLockedWidget(ui->gpu_backend_widget);
    connect(ui->language_comb, &QComboBox::currentIndexChanged, this, [this]() {
        if (m_loadingSettings) return;
        AppConfig& c = ConfigManager::instance().config();
        c.sherpa.languageModel = currentLanguageKey();
        c.sherpa.localModelRepoId = ui->model_list_widget->currentModel();
        applyDictionaryRules(true);
        applySettings(kCaption);
    });
    connect(ui->gpu_backend_widget, &GpuBackendWidget::computeModeChanged, this, [this](GpuBackendWidget::ComputeMode mode) {
        const bool toGpu = mode == GpuBackendWidget::ComputeMode::CUDA;
        // 列表里的 GPU 提示只跟显示模式走，载入配置时也要同步，所以放在 m_loadingSettings 判断之前
        ui->model_list_widget->setGpuMode(toGpu);
        if (m_loadingSettings) return;
        ConfigManager::instance().config().sherpa.useGpu = toGpu;
        applySettings(0);
        onComputeModeChanged(toGpu);
    });

    s->bind(ui->enable_enhancement_checkbox,
        [](const AppConfig& c) { return c.polish.enableAiEnhancement; },
        [](AppConfig& c, bool v) { c.polish.enableAiEnhancement = v; },
        Opt{ Apply::Instant });
    // 引擎没配置好时听写会静默用原文；只在打开开关时提醒一次，不逐句弹窗
    connect(ui->enable_enhancement_checkbox, &QAbstractButton::toggled, this, [this](bool on) {
        if (!on || m_loadingSettings) return;
        const PolishConfig& p = ConfigManager::instance().config().polish;
        if (p.usesLocalTranslation() || p.cloudEngineConfigured()) return;
        notify(NotifyLevel::Warning, QStringLiteral("AI 增强尚未配置"),
               p.aiEngineIndex == 0 ? QStringLiteral("还没有填写 Gemini API Key，润色和翻译不会生效，听写结果保持原文")
                                    : QStringLiteral("还没有填写 API 地址，润色和翻译不会生效，听写结果保持原文"));
    });
    s->bindIndex(ui->ai_engine_comb,
        [](const AppConfig& c) { return c.polish.aiEngineIndex; },
        [](AppConfig& c, int v) { c.polish.aiEngineIndex = v; },
        Opt{ Apply::Instant });
    s->bindText(ui->ai_model_comb,
        [](const AppConfig& c) { return c.polish.model; },
        [](AppConfig& c, const QString& v) { c.polish.model = v; },
        Opt{ Apply::Instant });
    s->bindData(ui->ai_style_comb,
        [](const AppConfig& c) { return c.polish.aiStyle; },
        [](AppConfig& c, const QString& v) { c.polish.aiStyle = v; },
        Opt{ Apply::Instant });
    s->bindData(ui->translate_language_comb,
        [](const AppConfig& c) { return c.polish.targetLang; },
        [](AppConfig& c, const QString& v) { c.polish.targetLang = v; },
        Opt{ Apply::Instant });
    s->bind(ui->translate_enabled_checkbox,
        [](const AppConfig& c) { return c.polish.translateEnabled; },
        [](AppConfig& c, bool v) { c.polish.translateEnabled = v; },
        Opt{ Apply::Instant });
    s->bindData(ui->translate_source_comb,
        [](const AppConfig& c) { return c.polish.translateSourceLang; },
        [](AppConfig& c, const QString& v) { c.polish.translateSourceLang = v; },
        Opt{ Apply::Instant });
    s->bind(ui->allow_self_signed_checkbox,
        [](const AppConfig& c) { return c.polish.allowSelfSigned; },
        [](AppConfig& c, bool v) { c.polish.allowSelfSigned = v; },
        Opt{ Apply::Instant });
    s->bind(ui->api_url_edit,
        [](const AppConfig& c) { return c.polish.apiUrl; },
        [](AppConfig& c, const QString& v) { c.polish.apiUrl = v; },
        Opt{ Apply::OnCommit });
    s->bind(ui->api_key_edit,
        [](const AppConfig& c) { return c.polish.apiKey; },
        [](AppConfig& c, const QString& v) { c.polish.apiKey = v; },
        Opt{ Apply::OnCommit });
    s->bind(ui->custom_commands_edit,
        [](const AppConfig& c) { return c.polish.prompt; },
        [](AppConfig& c, const QString& v) { c.polish.prompt = v; },
        Opt{ Apply::OnCommit });

    s->addLockedWidget(m_fileWidget);   // 与听写共用转录服务，不能同时进行

    connect(s, &SettingsBinder::changed, this, &MainWin::applySettings);
}

void MainWin::applySettings(unsigned effects)
{
    // 写文件不放在这一帧：杀毒扫描时一次写入能卡几十毫秒，导致动画掉帧；连续改动合并，退出时 ~MainWin 再存一次
    if (!m_saveTimer) {
        m_saveTimer = new QTimer(this);
        m_saveTimer->setSingleShot(true);
        m_saveTimer->setInterval(300);
        connect(m_saveTimer, &QTimer::timeout, this, []() {
            if (!ConfigManager::instance().save()) LOG_ERROR("Save Configuration Failed");
        });
    }
    m_saveTimer->start();
    const AppConfig& cfg = ConfigManager::instance().config();

    if (effects & SettingsBinder::Recorder) m_workflow->applyRecorderConfig();
    if (effects & SettingsBinder::LiveCaption) {
        if (cfg.liveCaptionEnabled.load()) m_sherpaInstaller->ensureStreamingModel(cfg.sherpa.languageModel);
        syncLiveCaption();
    }
}
