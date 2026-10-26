#include "MainWin.h"
#include "ui_MainWin.h"

#include <QActionGroup>
#include <QClipboard>
#include <algorithm>
#include <QDebug>
#include <QLocale>
#include <QSet>
#include <QDir>
#include <QFile>
#include <QStyleFactory>
#include <QTextStream>
#include <QThread>
#include <QTimer>
#include <QCheckBox>
#include <QListWidget>
#include <QDesktopServices>
#include <QScreen>
#include <QSignalBlocker>

#include "AppConfig.h"
#include "WorkflowManager.h"
#include "RecorderService.h"
#include "TranscriptionService.h"
#include "TextPolishService.h"
#include "TranslationService.h"
#include "translation/TranslationModelInstaller.h"
#include "translation/TranslationModelStore.h"
#include "translation/LanguageRouting.h"
#include "utils/AppPaths.h"
#include "ConfigManager.h"
#include "UpdateManager.h"
#include "TermsLibraryManager.h"
#include "sherpa/SherpaManager.h"
#include "sherpa/LanguageCatalog.h"
#include "utils/Logger.h"

#include "sherpa/StreamingCaptionEngine.h"
#include "widgets/LiveOverlay.h"
#include "widgets/LiveCaptionModel.h"
#include "widgets/SettingsBinder.h"
#include "widgets/DictationHistoryModel.h"
#include "widgets/DictationLogWidget.h"
#include "widgets/AboutDialog.h"
#include "widgets/FileTranscribeWidget.h"
#include "widgets/NavListWidget.h"
#include "widgets/inforbar/inforbarmanager.h"
#include "widgets/inforbar/inforposmanager.h"
#include "widgets/inforbar/inforbar.h"

MainWin::MainWin(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWin) {
  ui->setupUi(this);

  ui->ai_vocabulary_edit->setReadOnly(true);
  ui->config_log_edit->setReadOnly(true);

  ui->grop_key_label->setVisible(false);
  ui->grop_key_edit->setVisible(false);
  ui->gladia_key_edit->setVisible(false);
  ui->gladia_key_label->setVisible(false);
  ui->gemini_key_edit->setVisible(false);
  ui->gemini_key_label->setVisible(false);
  ui->grop_key_test_btn->setVisible(false);
  ui->gladia_key_test_btn->setVisible(false);
  ui->gemini_key_test_btn->setVisible(false);

  initSystemTray();
  initialize();
  setupUiConnections();
  registerSettings();
  onLoadConfig();
  alignSettingsCards();
}

void MainWin::alignSettingsCards()
{
    const QList<QGridLayout*> grids{ ui->device_card_layout, ui->recording_card_layout, ui->live_caption_card_layout,
                                     ui->shortcut_card_layout };
    for (int col : { 0, 3 }) {
        int width = 0;
        for (QGridLayout* grid : grids) {
            for (int i = 0; i < grid->count(); ++i) {
                // 只算正好占这一列的控件
                int r, c, rowSpan, colSpan;
                grid->getItemPosition(i, &r, &c, &rowSpan, &colSpan);
                if (c == col && colSpan == 1 && grid->itemAt(i)->widget())
                    width = std::max(width, grid->itemAt(i)->widget()->sizeHint().width());
            }
        }
        for (QGridLayout* grid : grids) grid->setColumnMinimumWidth(col, width);
    }
    // 第 2 列是左右两组之间的空隔
    for (QGridLayout* grid : grids) grid->setColumnMinimumWidth(2, 24);
}
    
void MainWin::initialize() {
    ui->recording_equ_comb->clear();
    auto items = AudioRecorderService::availableMicrophones();
    ui->recording_equ_comb->addItems(items);

    ui->source_equipment_comb->clear();
    auto speakers = AudioRecorderService::availableSpeakers();
    ui->source_equipment_comb->addItems(speakers);

    setupLanguageCombo();
    ui->model_list_widget->setLanguage(currentLanguageKey());

    ui->nav_list_widget->addNavItem("identification", "", {});
    ui->nav_list_widget->addNavItem("config", "", {});
    ui->nav_list_widget->addNavItem("download", "", {});
    retranslateDynamicItems();

    QActionGroup* styleActionGroup = new QActionGroup(this);
    styleActionGroup->addAction(ui->action_light);
    styleActionGroup->addAction(ui->action_gray);
    styleActionGroup->addAction(ui->action_dark);

    QActionGroup* langActionGroup = new QActionGroup(this);
    langActionGroup->addAction(ui->actionSystem);
    langActionGroup->addAction(ui->actionChinese);
    langActionGroup->addAction(ui->actionEnglish);

    m_networkManager = new QNetworkAccessManager(this);

    const AppConfig& config = ConfigManager::instance().config();
    m_textPolishService = new TextPolishService(m_networkManager, this);
    // 本地翻译后端尚未接入，传空时翻译请求直接回退原文
    m_translationService = new TranslationService(nullptr, this);
    {
        QFile manifestFile(QStringLiteral(":/translation_models.json"));
        manifestFile.open(QIODevice::ReadOnly);
        m_translationInstaller = new TranslationModelInstaller(
            m_networkManager, AppPaths::translationModelsDir(),
            TranslationModelStore::parseBundledManifest(manifestFile.readAll()), this);
    }
    m_recorderService = new AudioRecorderService(config, this);
    m_sherpaManager = new SherpaManager(this);
    m_sherpaInstaller = new SherpaInstaller(m_networkManager, this);
    m_cudaInstaller = new CudaInstaller(m_networkManager, m_sherpaInstaller, this);

    m_transcriptionService = new TranscriptionService(m_networkManager, m_sherpaManager, m_textPolishService,
                                                      m_translationService, config, this);
    m_termsManager = new TermsLibraryManager(this);
    m_updateManager = new UpdateManager(m_networkManager, this);

    m_workflow = new WorkflowManager(config, this);
    m_workflow->initialize(m_recorderService, m_transcriptionService, m_sherpaManager);

    // 初始化动画
    m_drawerAnimation = new QPropertyAnimation(ui->drawer_stack, "maximumHeight", this);
    m_drawerAnimation->setDuration(300);
    m_drawerAnimation->setEasingCurve(QEasingCurve::InOutQuart);

    // 声音控件
    m_liveOverlay = new LiveOverlay();
    m_captionModel = new LiveCaptionModel(this);
    m_history = new DictationHistoryModel(this);
    m_captionEngine = new StreamingCaptionEngine();
    m_liveOverlay->setCaptionModel(m_captionModel);
    m_captionModel->setTextFilter([](const QString& text) {
        return TermsLibraryManager::applyReplaceRules(text, ConfigManager::instance().config().replaceRules);
    });

    // 启动时检查一次更新
    QTimer::singleShot(2000, this, [this]() { 
        if (ConfigManager::instance().config().skipUpdateReminder) {
            LOG_INFO("Update available but skipped per user preference");
            return;
        }
        m_updateManager->checkForUpdates(); 
    });

    ui->download_list_widget->setSherpaInstaller(m_sherpaInstaller);
    ui->download_list_widget->setCudaInstaller(m_cudaInstaller);
    ui->download_list_widget->setTranslationInstaller(m_translationInstaller);
    // VAD 模型随安装包分发；若缺失(旧安装包/被误删)则自动补下载，避免用户卡在
    // "VAD 模型缺失，请先下载模型" 提示上却无从下手。
    m_sherpaInstaller->ensureVadModel();
    ui->gpu_backend_widget->setBackendInstaller(m_cudaInstaller);
    ui->terms_widget->setTermsManager(m_termsManager);
    ui->file_transcribe_widget->setServices(m_transcriptionService, m_sherpaManager, &ConfigManager::instance().config());
}  

void MainWin::setupUiConnections(){
    auto& configManager = ConfigManager::instance();

	connect(ui->actionAbout, &QAction::triggered, this, [this]() {
        AboutDialog dialog(this);
        dialog.exec();
	});

    // Basic Setting
	connect(ui->shortcut_edit, &ShortcutEdit::hotkeyActivated, this, &MainWin::onHotkeyPressed);
	connect(ui->shortcut_edit, &ShortcutEdit::hotkeyReleased, this, &MainWin::onHotkeyReleased);

    // 关掉实时字幕时字号和位置置灰，数值保留
    connect(ui->live_caption_checkbox, &QCheckBox::toggled, this, [this](bool on) {
        const QList<QWidget*> options{ ui->label_live_caption_font, ui->live_caption_font_spin,
                                       ui->label_live_caption_position, ui->live_caption_position_comb };
        for (QWidget* w : options) w->setEnabled(on);
    });

    // 只显示当前云端后端的 Key
    connect(ui->identification_backend_comb, &QComboBox::currentIndexChanged, this, [this](int index) {
        const auto backend = static_cast<AsrBackendKind>(index);
        const struct { AsrBackendKind kind; QList<QWidget*> row; } keyRows[] = {
            { AsrBackendKind::Groq,   { ui->grop_key_label, ui->grop_key_edit, ui->grop_key_test_btn } },
            { AsrBackendKind::Gladia, { ui->gladia_key_label, ui->gladia_key_edit, ui->gladia_key_test_btn } },
            { AsrBackendKind::Gemini, { ui->gemini_key_label, ui->gemini_key_edit, ui->gemini_key_test_btn } },
        };
        // 避免出现闪烁的问题
        QWidget* page = ui->settingsScrollArea;
        page->setUpdatesEnabled(false);
        for (const auto& r : keyRows) {
            for (QWidget* w : r.row) w->setVisible(r.kind == backend);
        }
        QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
        page->setUpdatesEnabled(true);
    });

    const struct { QPushButton* btn; QLineEdit* edit; AsrBackendKind kind; } keyTests[] = {
        { ui->grop_key_test_btn,   ui->grop_key_edit,   AsrBackendKind::Groq },
        { ui->gladia_key_test_btn, ui->gladia_key_edit, AsrBackendKind::Gladia },
        { ui->gemini_key_test_btn, ui->gemini_key_edit, AsrBackendKind::Gemini },
    };
    for (const auto& t : keyTests) {
        connect(t.btn, &QPushButton::clicked, this, [this, btn = t.btn, edit = t.edit, kind = t.kind]() {
            btn->setEnabled(false);
            m_transcriptionService->testKey(kind, edit->text().trimmed(), [this, btn](bool ok, const QString& cause) {
                btn->setEnabled(true);
                if (ok) notify(NotifyLevel::Success, tr("Key 可用"));
                else notify(NotifyLevel::Error, tr("Key 测试失败"), cause);
            });
        });
    }

    // 输出设备测试播放
    connect(ui->test_output_btn, &QPushButton::clicked, this, [this]() {
        QString name = ui->source_equipment_comb->currentText();
        m_workflow->playTestTone();
    });

    // Local Recognition
    connect(ui->uninstall_sherpa_btn, &QPushButton::clicked, this, [this]() {
        m_sherpaInstaller->uninstallAll();
        ui->model_list_widget->refreshInstalled();
    });

	connect(ui->language_comb, &QComboBox::currentIndexChanged, this, [this](int index) {
		// 仅刷新列表并默认选中第一个模型，不触发下载
		ui->model_list_widget->setLanguage(ui->language_comb->itemData(index).toString());
	});

	// 下载触发：仅用户在模型列表里点击时下载
	connect(ui->model_list_widget, &ModelListWidget::modelActivated, this, &MainWin::onLocalModelActivated);

    connect(m_sherpaInstaller, &SherpaInstaller::installGroupStarted, this, [=]() {
        notify(NotifyLevel::Info, tr("开始下载模型..."));
    });
    connect(m_sherpaInstaller, &SherpaInstaller::installGroupFinished, this, [=](const QString&, bool success, const QString&) {
        ui->model_list_widget->refreshInstalled();
        if (success) notify(NotifyLevel::Success, tr("模型下载完成"));
        else notify(NotifyLevel::Error, tr("模型下载失败"), tr("请检查网络后重试"));
    });
    connect(m_sherpaInstaller, &SherpaInstaller::vadModelReady, this, [this](bool success, const QString&) {
        if (success) {
            LOG_INFO("VAD model ready, rebuilding VAD detector");
            m_workflow->applyRecorderConfig();
        } else {
            notify(NotifyLevel::Error, tr("VAD 模型下载失败"), tr("请检查网络后重试，或前往 \"下载列表\" 查看进度"));
        }
    });
    // CUDA 初始化失败回退 CPU（非错误，仅提示）
    connect(m_sherpaManager, &SherpaManager::gpuFallbackToCpu, this, [this]() {
        notify(NotifyLevel::Warning, tr("已切换至 CPU 模式"), tr("显卡不可用，识别速度可能下降"));
    });

    connect(ui->open_path_btn, &QPushButton::clicked, this, [this]() {
        QString modelPath = ModelConfigFactory::getSherpaModel();
        QString nativePath = QDir::toNativeSeparators(modelPath);
        QUrl url = QUrl::fromLocalFile(nativePath);
        bool success = QDesktopServices::openUrl(url);
        if (!success) {
            LOG_ERROR(QString("Failed to open path! Original: %1, converted URL: %2").arg(modelPath).arg(url.toString()));
        }
    });

    connect(ui->gpu_backend_widget, &GpuBackendWidget::detectFinished, this, [this](bool result) {
        ConfigManager::instance().config().sherpa.useGpu = result;
        ConfigManager::instance().save();
    });

    // gemini
    connect(ui->check_connection_btn, &QPushButton::clicked, this, [this]() {
        const int engine = ui->ai_engine_comb->currentIndex();
        if (!m_textPolishService->capabilities(engine).canTest) return;

        const AppConfig& config = ConfigManager::instance().config();
        TextPolishService::RequestParams params;
        params.aiEngine = engine;
        params.apiKey = config.polish.apiKey;
        params.customUrl = config.polish.apiUrl;
        params.model = config.polish.model;
        params.style = config.polish.aiStyle;
        params.customPrompt = config.polish.prompt;
        params.aiVocab = config.polish.vocab;
        params.targetLang = config.polish.cloudTargetLang();
        m_textPolishService->testConnection(params);
    });

    connect(ui->nav_list_widget, &QListWidget::currentItemChanged, this, [this]() {
        onToggleDrawer(ui->nav_list_widget->currentRow());
    });

    connect(ui->enable_enhancement_checkbox, &QCheckBox::toggled, this, [this]() { updateAiEngineUi(); });

    connect(ui->ai_style_comb, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
        updateAiEngineUi();
    });
    connect(ui->translate_enabled_checkbox, &QCheckBox::toggled, this, [this](bool on) {
        AppConfig& cfg = ConfigManager::instance().config();
        // 下拉里没有"不翻译"项：旧配置里的"不翻译"在开启时换成当前显示的语言，否则开关开着却不翻译
        if (on && !m_loadingSettings && (cfg.polish.targetLang.isEmpty() || cfg.polish.targetLang == QStringLiteral("不翻译"))) {
            cfg.polish.targetLang = ui->translate_language_comb->currentData().toString();
            ConfigManager::instance().save();
        }
        updateAiEngineUi();
    });
    connect(ui->translate_source_comb, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
        if (!m_loadingSettings) updateTranslationUi();
    });
    connect(ui->translate_language_comb, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
        if (!m_loadingSettings) updateTranslationUi();
    });
    connect(ui->language_comb, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
        if (m_loadingSettings) return;
        rebuildTranslationLanguages();
        updateTranslationUi();
    });
    connect(ui->translate_model_download_btn, &QPushButton::clicked, this, [this]() {
        m_translationInstaller->install(currentTranslationRoute());
        updateTranslationUi();
    });
    connect(m_translationInstaller, &TranslationModelInstaller::installGroupStarted, this, [this]() { updateTranslationUi(); });
    connect(m_translationInstaller, &TranslationModelInstaller::installGroupFinished, this, [this]() { updateTranslationUi(); });

    connect(ui->ai_engine_comb, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        // 目标语言列表随引擎变（云端几种常用语言 / 本地按已有模型）；载入设置时由 loadConfigToUI 统一重建
        if (!m_loadingSettings) rebuildTranslationLanguages();
        updateAiEngineUi();
        // 载入设置时由 loadConfigToUI 在末尾统一拉取，避免重复请求
        if (m_loadingSettings) return;
        if (!m_textPolishService->capabilities(index).canFetchModels) return;

        const AppConfig& config = ConfigManager::instance().config();
        TextPolishService::RequestParams params;
        params.aiEngine = index;
        params.apiKey = config.polish.apiKey;
        params.customUrl = config.polish.apiUrl;
        m_textPolishService->fetchModels(params);
    });

    connect(m_textPolishService, &TextPolishService::modelsFetched, this, [this](bool success, const QStringList& models, const QString&) {
        if (!success || models.isEmpty()) {
            LOG_ERROR("Failed to fetch model list");
            ui->ai_model_comb->clear();
            return;
        }

        LOG_INFO("Fetched model list successfully");
        // 重建列表时不能让下拉框的改动信号把已保存的模型名冲掉
        AppConfig& config = ConfigManager::instance().config();
        {
            const QSignalBlocker block(ui->ai_model_comb);
            ui->ai_model_comb->clear();
            ui->ai_model_comb->addItems(models);
            ui->ai_model_comb->setCurrentIndex(std::max(0, ui->ai_model_comb->findText(config.polish.model)));
        }
        // 已保存的模型不在新列表里（如换了引擎）时，改用列表第一项
        if (config.polish.model != ui->ai_model_comb->currentText()) {
            config.polish.model = ui->ai_model_comb->currentText();
            applySettings(0);
        }
    });

    // 更新检查
    connect(ui->actionUpdate, &QAction::triggered, this, [this]() {  m_updateManager->checkForUpdates(); });
    connect(m_updateManager, &UpdateManager::updateAvailable, this, &MainWin::onUpdateFound);
    connect(m_updateManager, &UpdateManager::downloadFinished, this, &MainWin::onUpdateDownloaded);
    connect(m_updateManager, &UpdateManager::updateError, this, [this](const QString& message) { LOG_ERROR(message); });

    // 日志输出
    connect(&Logger::instance(), &Logger::newLogEntry, this, &MainWin::onNewLogEntry, Qt::QueuedConnection);

    // 主要工作流
    connect(m_workflow, &WorkflowManager::transcriptionResultReady, m_history, [this](const QString& text) { m_history->add(text); });
    ui->dictation_log_widget->setModel(m_history);
    connect(ui->dictation_log_widget, &DictationLogWidget::copied, this, [this]() { notify(NotifyLevel::Success, tr("已复制")); });
    connect(m_workflow, &WorkflowManager::stateChanged, this, &MainWin::onStateChanged);
    connect(m_workflow, &WorkflowManager::errorOccurred, this, [this](const QString& title, const QString& cause) {
        notify(NotifyLevel::Error, title, cause);
    });
    connect(m_transcriptionService, &TranscriptionService::enhancementFailed, this,
            [this](const QString& cause, bool fileMode) {
                // 文件转录逐段失败会刷屏，只在听写时提示
                if (fileMode) return;
                notify(NotifyLevel::Warning, tr("润色失败，已使用原文"), cause);
            });
    connect(m_translationInstaller, &TranslationModelInstaller::installFailed, this,
            [this](const QString&, const QString&, const QString& cause) {
                notify(NotifyLevel::Error, tr("翻译模型安装失败"), cause);
            });
    connect(m_transcriptionService, &TranscriptionService::translationFailed, this,
            [this](const QString& cause) {
                notify(NotifyLevel::Warning, tr("翻译失败，已使用原文"), cause);
            });

    // 录音服务：可视化数据/活动通道直连 overlay（不经门面）
    connect(m_recorderService, &AudioRecorderService::levelUpdated, m_liveOverlay, &LiveOverlay::setLevel);
    connect(m_recorderService, &AudioRecorderService::spectrumUpdated, m_liveOverlay, &LiveOverlay::setSpectrumLevels);
    connect(m_recorderService, &AudioRecorderService::voiceStopped, m_liveOverlay, &LiveOverlay::onVoiceStopped);
    connect(m_recorderService, &AudioRecorderService::voiceStarted, m_liveOverlay, &LiveOverlay::onVoiceStarted);

    // 实时字幕：音频 → 流式引擎(独立线程) → 中间结果 → 字幕模型；正式结果到达后替换并变深色
    connect(m_recorderService, &AudioRecorderService::audioChunk, m_captionEngine, &StreamingCaptionEngine::acceptAudio);
    connect(m_captionEngine, &StreamingCaptionEngine::partialText, m_captionModel, &LiveCaptionModel::setPartial);
    connect(m_captionModel, &LiveCaptionModel::epochChanged, m_captionEngine, &StreamingCaptionEngine::resetStream);
    connect(m_captionEngine, &StreamingCaptionEngine::modelReady, this, [this](const QString&, bool ok) {
        m_recorderService->setAudioTapEnabled(ok && ConfigManager::instance().config().liveCaptionEnabled.load());
        // 每次开始录音都会重试装载，只提示一次，免得每次录音都弹窗
        if (!ok && !m_liveCaptionFailNotified) {
            m_liveCaptionFailNotified = true;
            notify(NotifyLevel::Warning, tr("实时字幕不可用"), tr("流式识别模型加载或解码失败，听写不受影响"));
        }
    });
    connect(m_recorderService, &AudioRecorderService::voiceStarted, m_captionModel, [this]() { m_captionModel->setSpeaking(true); });
    connect(m_recorderService, &AudioRecorderService::voiceStopped, m_captionModel, [this]() { m_captionModel->setSpeaking(false); });
    connect(m_workflow, &WorkflowManager::utteranceAccepted, m_captionModel, [this](quint64 id) { m_captionModel->onUtteranceReady(id); });
    connect(m_workflow, &WorkflowManager::utteranceFinished, m_captionModel,
            [this](const QString& text, quint64 id) { m_captionModel->onTranscribed(id, text); });
    connect(m_sherpaInstaller, &SherpaInstaller::streamingModelReady, this, &MainWin::syncLiveCaption);
    connect(m_sherpaManager, &SherpaManager::idleUnloaded, this, [this]() {
        m_recorderService->setAudioTapEnabled(false);
        QMetaObject::invokeMethod(m_captionEngine, &StreamingCaptionEngine::unloadModel, Qt::QueuedConnection);
    });

    // Sherpa 模型安装器：安装成功后经门面预热模型（不再直戳 m_sherpaManager）
    connect(m_sherpaInstaller, &SherpaInstaller::loadModel,
        this, [this](const QString& repoId, bool success, const QString& msg) {
            if (success && !repoId.isEmpty()) {
                const AppConfig& config = ConfigManager::instance().config();
                m_workflow->preloadModel(config);
                notify(NotifyLevel::Info, tr("模型加载成功"));
            }
        });

    connect(m_liveOverlay, &LiveOverlay::sphereClicked, this, [this]() {
        m_workflow->togglePause();
        if (m_recorderService->isPaused()) {
            m_liveOverlay->setPaused();
        } else {
            m_liveOverlay->setListening();
        }
    });

    // 词典功能
    connect(m_termsManager, &TermsLibraryManager::termsReloaded, this, [=]() {
        auto rules = m_termsManager->buildAllRules();
		AppConfig& appConfig = ConfigManager::instance().config();
        appConfig.replaceRules = rules.replaceRules;
        appConfig.polish.vocab = rules.aiVocab;

		// 热词表
        QString hotwords;
        for (const QString& w : rules.hotwords) hotwords += w + '\n';
        appConfig.sherpa.hotwords = hotwords;
		ConfigManager::instance().save();

        ui->ai_vocabulary_edit->setText(appConfig.polish.vocab);
    });

    connect(ui->terms_widget, &TermWidget::errorOccurred, this, [this](const QString& title, const QString& cause) {
        NotifyLevel level = title.contains(tr("成功")) ? NotifyLevel::Success
                            : title.contains(tr("冲突")) ? NotifyLevel::Warning
                            : NotifyLevel::Error;
        notify(level, title, cause);
    });

    connect(ui->file_transcribe_widget, &FileTranscribeWidget::errorOccurred, this,
            [this](const QString& title, const QString& cause) { notify(NotifyLevel::Error, title, cause); });

}

MainWin::~MainWin() {
  // 字幕引擎自带线程，先于 UI 收掉
  delete m_captionEngine;
  m_captionEngine = nullptr;
  ConfigManager::instance().save();
  delete ui;
}

void MainWin::syncLiveCaption()
{
    if (!m_captionEngine) return;
    const AppConfig& cfg = ConfigManager::instance().config();
    m_liveOverlay->setCaptionFontSize(cfg.liveCaptionFontSize);
    m_liveOverlay->setAtTop(cfg.liveCaptionAtTop);

    const QString repoId = ModelRegistry::StreamingModelFor(cfg.sherpa.languageModel);
    const bool on = cfg.liveCaptionEnabled.load() && !repoId.isEmpty()
        && SherpaInstaller::isInstalled(repoId) && !m_sherpaInstaller->isInstalling(repoId);

    if (on) {
        // 音频抽头等引擎报告 modelReady 之后再开，避免白白投递数据
        // 主模型优先：字幕最多用一半线程，且不超过 2
        const int threads = std::clamp(cfg.sherpa.threads / 2, 1, 2);
        QMetaObject::invokeMethod(m_captionEngine, [engine = m_captionEngine, repoId, threads]() {
            engine->loadModel(repoId, threads);
        }, Qt::QueuedConnection);
    } else {
        m_recorderService->setAudioTapEnabled(false);
        QMetaObject::invokeMethod(m_captionEngine, &StreamingCaptionEngine::unloadModel, Qt::QueuedConnection);
        m_captionModel->clear();
    }
}

void MainWin::changeEvent(QEvent* event)
{
    if (QEvent::LanguageChange == event->type()) {
        ui->retranslateUi(this);
        // 代码里动态加的下拉项和导航，retranslateUi 不会更新
        retranslateDynamicItems();
        setDictationIndicator(m_indicatorRecording);   // retranslateUi 会把状态文字重置为默认值
        refreshLanguageComboTexts();
        alignSettingsCards();
    }
    else if (event->type() == QEvent::WindowStateChange) {
        if (windowState() & Qt::WindowMinimized) {
            hide();  
        }
    }
    QMainWindow::changeEvent(event);
}


void MainWin::closeEvent(QCloseEvent* event)
{
    if (trayIcon && trayIcon->isVisible()) {
        hide();
        setWindowState(Qt::WindowMinimized);
        event->ignore();  
    }
    else {
        event->accept();
    }
}


void MainWin::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (!m_fittedToScreen) {
        m_fittedToScreen = true;
        // 首次显示时下拉框已填充、翻译已加载，尺寸才是最终值；
        // 推迟到下一轮事件循环，原生窗口装好标题栏后 frameGeometry 才准确
        QTimer::singleShot(0, this, &MainWin::fitToScreen);
    }
}

void MainWin::fitToScreen()
{
    QScreen* screen = this->screen();
    if (!screen) return;
    const QRect avail = screen->availableGeometry();
    const QSize frameExtra = frameGeometry().size() - size();   // 标题栏与边框

    LOG_DEBUG(QString("Window fit | avail=%1x%2 window=%3x%4 min=%5x%6 frame=%7x%8")
                  .arg(avail.width()).arg(avail.height())
                  .arg(width()).arg(height())
                  .arg(minimumSizeHint().width()).arg(minimumSizeHint().height())
                  .arg(frameExtra.width()).arg(frameExtra.height()));

    const int maxW = avail.width() - frameExtra.width();
    const int maxH = avail.height() - frameExtra.height();
    if (width() > maxW || height() > maxH) {
        resize(qMin(width(), maxW), qMin(height(), maxH));
    }

    // 尺寸缩不下（最小尺寸大于屏幕）时也至少保证左上角可见，让标题栏能拖动
    QRect frame = frameGeometry();
    const int x = qBound(avail.left(), frame.left(), qMax(avail.left(), avail.right() - frame.width() + 1));
    const int y = qBound(avail.top(), frame.top(), qMax(avail.top(), avail.bottom() - frame.height() + 1));
    if (x != frame.left() || y != frame.top()) {
        move(x, y);
    }
}

void MainWin::onLocalModelActivated()
{
	AppConfig& config = ConfigManager::instance().config();
	config.sherpa.languageModel = currentLanguageKey();
	config.sherpa.localModelRepoId = ui->model_list_widget->currentModel();
	applySettings(0);

	QString repoId = ModelRegistry::FindByDisplayName(config.sherpa.languageModel,
		config.sherpa.localModelRepoId);
	if (repoId.isEmpty()) {
		LOG_ERROR("Model not found");
		return;
	}
	if (m_sherpaInstaller->isInstalling(repoId)) {
		LOG_INFO(QString("Model installing: %1").arg(repoId));
		notify(NotifyLevel::Info, tr("该模型正在安装...."));
		return;
	}

	LOG_DEBUG(QString("Download %1...").arg(repoId));
	m_sherpaInstaller->installModel(repoId);
}

// 启动时：配置 → 控件。标准控件由 SettingsBinder 回填，其余是登记表里注明的自定义流程
void MainWin::loadConfigToUI() {
    ConfigManager::instance().load();
    AppConfig& cfg = ConfigManager::instance().config();

    m_loadingSettings = true;
    m_settings->loadAll();
    ui->shortcut_edit->setShortCut(cfg.hotkey);
    ui->gpu_backend_widget->setComputeMode(cfg.sherpa.useGpu ? GpuBackendWidget::ComputeMode::CUDA : GpuBackendWidget::ComputeMode::CPU);
    const int index = ui->language_comb->findData(cfg.sherpa.languageModel);
    if (index != -1) ui->language_comb->setCurrentIndex(index);
    ui->model_list_widget->setCurrentModel(cfg.sherpa.localModelRepoId);
    ui->ai_vocabulary_edit->setText(cfg.polish.vocab);
    rebuildTranslationLanguages();   // 下拉内容依赖已载入的翻译引擎，要在 loadAll 之后按配置重选
    m_loadingSettings = false;

    // 首次运行（未配置）：默认选中第一项，确保启动时能尝试加载
    if (cfg.sherpa.languageModel.isEmpty() && !currentLanguageKey().isEmpty())
        cfg.sherpa.languageModel = currentLanguageKey();
    if (cfg.sherpa.localModelRepoId.isEmpty() && !ui->model_list_widget->currentModel().isEmpty())
        cfg.sherpa.localModelRepoId = ui->model_list_widget->currentModel();

    if (cfg.backend == AsrBackendKind::Sherpa) {
        QString repoId = ModelRegistry::FindByDisplayName(cfg.sherpa.languageModel,
                                                          cfg.sherpa.localModelRepoId);
        if (!repoId.isEmpty()) {
            m_sherpaInstaller->installModel(repoId);
            m_workflow->preloadModel(cfg);
        }
    }

    updateAiEngineUi();

    const int engine = ui->ai_engine_comb->currentIndex();
    if (!m_textPolishService->capabilities(engine).canFetchModels) return;

    TextPolishService::RequestParams params;
    params.aiEngine = engine;
    params.apiKey = cfg.polish.apiKey;
    params.customUrl = cfg.polish.apiUrl;
    m_textPolishService->fetchModels(params);
}

namespace {

// key 为空表示该下拉按序号存取；非空时作为 itemData，配置里存的是它，不随界面语言变。
// 风格和翻译目标语言的 key 也会写进提示词，所以保持原来的中文值。
struct ComboEntry {
    const char* label;
    const char* key;
};

const ComboEntry kBackendItems[] = {
    { QT_TRANSLATE_NOOP("MainWin", "Local (Sherpa)"), "" },
    { QT_TRANSLATE_NOOP("MainWin", "Gemini voice input"), "" },
    { QT_TRANSLATE_NOOP("MainWin", "Groq voice input"), "" },
    { QT_TRANSLATE_NOOP("MainWin", "Gladia voice input"), "" },
};

const ComboEntry kAiEngineItems[] = {
    { QT_TRANSLATE_NOOP("MainWin", "Google Gemini API"), "" },
    { QT_TRANSLATE_NOOP("MainWin", "Local / custom (OpenAI compatible)"), "" },
    { QT_TRANSLATE_NOOP("MainWin", "Ollama (local)"), "" },
    { QT_TRANSLATE_NOOP("MainWin", "Local model (offline translation)"), "" },
};

const ComboEntry kStyleItems[] = {
    { QT_TRANSLATE_NOOP("MainWin", "Smart default"), "智慧预设" },
    { QT_TRANSLATE_NOOP("MainWin", "Business formal"), "商务正式" },
    { QT_TRANSLATE_NOOP("MainWin", "Casual spoken"), "日常口語" },
    { QT_TRANSLATE_NOOP("MainWin", "Concise"), "简洁扼要" },
    { QT_TRANSLATE_NOOP("MainWin", "Custom prompt"), "自订Prompt" },
};

// 云端引擎的目标语言：键是中文名，会写进提示词
const ComboEntry kTargetLangItems[] = {
    { QT_TRANSLATE_NOOP("MainWin", "English"), "英文" },
    { QT_TRANSLATE_NOOP("MainWin", "Japanese"), "日文" },
    { QT_TRANSLATE_NOOP("MainWin", "Korean"), "韩文" },
    { QT_TRANSLATE_NOOP("MainWin", "Traditional Chinese"), "繁体中文" },
    { QT_TRANSLATE_NOOP("MainWin", "Simplified Chinese"), "简体中文" },
};

// 首次调用添加条目，之后只更新显示文字（setItemText 不会触发 currentIndexChanged，不会误写配置）
template <size_t N>
void fillCombo(QComboBox* combo, const ComboEntry (&entries)[N]) {
    for (size_t i = 0; i < N; ++i) {
        const QString text = QCoreApplication::translate("MainWin", entries[i].label);
        if (int(i) < combo->count()) combo->setItemText(int(i), text);
        else combo->addItem(text, QString::fromUtf8(entries[i].key));
    }
}

} // namespace

void MainWin::retranslateDynamicItems() {
    fillCombo(ui->identification_backend_comb, kBackendItems);
    fillCombo(ui->ai_engine_comb, kAiEngineItems);
    fillCombo(ui->ai_style_comb, kStyleItems);
    rebuildTranslationLanguages();

    ui->nav_list_widget->setNavText("identification", tr("Recognition"));
    ui->nav_list_widget->setNavText("config", tr("Logs & Config"));
    ui->nav_list_widget->setNavText("download", tr("Downloads"));
}

namespace {

QString translationLanguageLabel(const QString& code) {
    if (code == QLatin1String("zh-Hans")) return QCoreApplication::translate("MainWin", "Simplified Chinese");
    if (code == QLatin1String("zh-Hant")) return QCoreApplication::translate("MainWin", "Traditional Chinese");
    const QString key = LanguageCatalog::keyOfCode(code);
    if (!key.isEmpty()) return LanguageCatalog::displayName(key);
    return QStringLiteral("%1 (%2)").arg(QLocale::languageToString(QLocale(code).language()), code);
}

bool isCoreTranslationLanguage(const QString& code) {
    return !displayNameForLanguageCode(code).isEmpty();
}

} // namespace

// 识别模型的语言代码，源语言选"自动"时据此推断
QString MainWin::recognitionLanguageCode() const {
    return LanguageCatalog::codeOf(currentLanguageKey());
}

// 目标语言和源语言下拉的内容随翻译引擎变化：云端只有常用几种，本地按已有模型方向生成。
// 当前值在新列表里找不到时只是界面回退到第一项，不改写配置里保存的值。
void MainWin::rebuildTranslationLanguages() {
    if (!m_translationInstaller) return;
    const AppConfig& cfg = ConfigManager::instance().config();
    const bool local = ui->ai_engine_comb->currentIndex() == kLocalTranslationEngine;

    QStringList codes;
    if (local) {
        QSet<QString> langs;
        for (const QString& pair : m_translationInstaller->knownPairs()) {
            for (const QString& code : pair.split(QLatin1Char('>'))) langs.insert(code);
        }
        for (const QString& core : QStringList{ "en", "zh-Hans", "zh-Hant", "ja", "ko" }) {
            if (langs.remove(core)) codes << core;
        }
        QStringList rest = langs.values();
        std::sort(rest.begin(), rest.end(), [](const QString& a, const QString& b) {
            return translationLanguageLabel(a).localeAwareCompare(translationLanguageLabel(b)) < 0;
        });
        codes += rest;
    }

    {
        const QSignalBlocker blocker(ui->translate_language_comb);
        ui->translate_language_comb->clear();
        if (local) {
            for (const QString& code : codes) {
                const QString text = translationLanguageLabel(code);
                // 核心五种沿用中文名作为键，和云端引擎的配置值通用；其余直接存语言代码
                const QString core = displayNameForLanguageCode(code);
                ui->translate_language_comb->addItem(text, core.isEmpty() ? code : core);
                if (!isCoreTranslationLanguage(code)) {
                    ui->translate_language_comb->setItemData(ui->translate_language_comb->count() - 1,
                                                             tr("Quality not evaluated"), Qt::ToolTipRole);
                }
            }
        }
        else {
            for (const ComboEntry& e : kTargetLangItems) {
                ui->translate_language_comb->addItem(QCoreApplication::translate("MainWin", e.label),
                                                     QString::fromUtf8(e.key));
            }
        }
        const int saved = ui->translate_language_comb->findData(cfg.polish.targetLang);
        ui->translate_language_comb->setCurrentIndex(saved >= 0 ? saved : 0);
    }

    {
        const QSignalBlocker blocker(ui->translate_source_comb);
        ui->translate_source_comb->clear();
        ui->translate_source_comb->addItem(tr("Auto"), QStringLiteral("auto"));
        for (const QString& code : codes) ui->translate_source_comb->addItem(translationLanguageLabel(code), code);
        const int saved = ui->translate_source_comb->findData(cfg.polish.translateSourceLang);
        ui->translate_source_comb->setCurrentIndex(saved >= 0 ? saved : 0);
    }
}

// 本地翻译模型的状态文字与下载按钮：未安装显示大小，已装显示就绪；不自动下载
TranslationRoute MainWin::currentTranslationRoute() const {
    const QString from = resolveSourceCode(ui->translate_source_comb->currentData().toString(), recognitionLanguageCode());
    const QString to = languageCodeForStored(ui->translate_language_comb->currentData().toString());
    return planRoute(from, to, m_translationInstaller->knownPairs());
}

void MainWin::updateTranslationUi() {
    if (!m_translationInstaller) return;
    const bool local = ui->ai_engine_comb->currentIndex() == kLocalTranslationEngine;
    const bool transOn = ui->translate_enabled_checkbox->isChecked();

    const QString from = resolveSourceCode(ui->translate_source_comb->currentData().toString(), recognitionLanguageCode());
    const TranslationRoute route = currentTranslationRoute();

    QString status;
    bool canDownload = false;
    if (route.kind == RouteKind::Same) {
        status = tr("Source and target are the same language");
    }
    else if (route.kind == RouteKind::Unsupported) {
        status = from.isEmpty() ? tr("Cannot detect the source language; choose one manually")
                                : tr("This language pair is not supported");
    }
    else if (m_translationInstaller->isRouteInstalled(route)) {
        status = tr("Model ready");
    }
    else {
        bool installing = false;
        for (const TranslationHop& hop : route.hops) installing |= m_translationInstaller->isInstalling(hop.from, hop.to);
        if (installing) {
            status = tr("Downloading model...");
        }
        else {
            const double mb = m_translationInstaller->bytesToDownload(route) / 1048576.0;
            status = tr("Model download required: %1 MB").arg(mb, 0, 'f', 0);
            canDownload = true;
        }
    }

    ui->translate_model_status_label->setText(status);
    ui->translate_model_download_btn->setEnabled(transOn && canDownload);

    ui->translate_model_status_label->setVisible(local);
    ui->translate_model_download_btn->setVisible(local);
}

// 润色与翻译相关控件只在这里启用/隐藏：开关关闭时灰显并保留值，"不适用"的块直接隐藏。
// 选本地模型时翻译在本机完成，没有润色，也不需要云端的 Key、地址和模型。
void MainWin::updateAiEngineUi() {
    const int engine = ui->ai_engine_comb->currentIndex();
    const bool local = engine == kLocalTranslationEngine;
    const bool polishOn = !local && ui->enable_enhancement_checkbox->isChecked();
    const bool transOn = ui->translate_enabled_checkbox->isChecked();
    const auto caps = m_textPolishService->capabilities(engine);

    const auto setEnabled = [](const QList<QWidget*>& widgets, bool on) {
        for (QWidget* w : widgets) w->setEnabled(on);
    };
    const auto setVisible = [](const QList<QWidget*>& widgets, bool on) {
        for (QWidget* w : widgets) w->setVisible(on);
    };

    // 引擎下拉始终显示；连接信息只对云端引擎有意义
    setVisible({ ui->label_13, ui->api_key_edit, ui->label_16, ui->api_url_edit,
                 ui->label_14, ui->ai_model_comb, ui->check_connection_btn }, !local);
    setEnabled({ ui->ai_model_comb, ui->label_14 }, caps.canFetchModels);
    ui->check_connection_btn->setEnabled(caps.canTest);

    ui->polish_card->setVisible(!local);
    // 自订指令只有风格选"自订Prompt"时才会用到，其余风格下显示出来没有作用
    const bool customStyle = ui->ai_style_comb->currentData().toString() == QStringLiteral("自订Prompt");
    setVisible({ ui->label_17, ui->custom_commands_edit }, customStyle);
    setEnabled({ ui->ai_style_comb, ui->custom_commands_edit, ui->ai_vocabulary_edit,
                 ui->label_18, ui->label_17, ui->label_20 }, polishOn && caps.canPolish);

    // 云端翻译随润色请求一起发出，润色关闭时无法使用
    const bool cloudBlocked = !local && !polishOn;
    const bool detailOn = transOn && !cloudBlocked;
    setEnabled({ ui->translate_source_comb, ui->translate_source_label,
                 ui->translate_language_comb, ui->label_19 }, detailOn);
    setVisible({ ui->translate_source_comb, ui->translate_source_label }, local);

    QString hint;
    if (transOn && cloudBlocked) {
        hint = tr("Enable polishing first, or switch to the local model");
    }
    ui->translate_hint_label->setText(hint);
    ui->translate_hint_label->setVisible(!hint.isEmpty());

    updateTranslationUi();

    // 只列出实际会执行的步骤；润色和翻译都没开时没有额外处理，不显示这一行
    QStringList steps{ tr("Recognition") };
    if (polishOn) steps << tr("Polishing");
    if (transOn && !cloudBlocked) steps << tr("Translation");
    steps << tr("Output");
    ui->pipeline_hint_label->setText(steps.join(QStringLiteral("  →  ")));
    ui->pipeline_hint_label->setVisible(steps.size() > 2);
}

void MainWin::readIni() {
  QString inifile = ConfigManager::configFilePath();
  if (!QFile::exists(inifile)) return;

  QSettings settings(inifile, QSettings::IniFormat);
  style = settings.value("style", "dark").toString();

  if (style == "dark") {
    QString style = "Fusion";
    qApp->setStyle(QStyleFactory::create(style));
    QFile styleFile(":/style/myblackstyle.qss");
    styleFile.open(QFile::ReadOnly | QFile::Text);
    QTextStream ts(&styleFile);
    qApp->setStyleSheet(ts.readAll());
    styleFile.close();

    ui->action_dark->setChecked(true);
    m_liveOverlay->setTheme(LiveOverlay::Theme::Dark);
  } else if (style == "gray") {
    QString style = "Fusion";
    qApp->setStyle(QStyleFactory::create(style));
    QFile styleFile(":/style/mygraystyle.qss");
    styleFile.open(QFile::ReadOnly | QFile::Text);
    QTextStream ts(&styleFile);
    qApp->setStyleSheet(ts.readAll());
    styleFile.close();

    ui->action_gray->setChecked(true);
    m_liveOverlay->setTheme(LiveOverlay::Theme::Gray);
  } else {
    QString style = "Fusion";
    qApp->setStyle(QStyleFactory::create(style));
    QFile styleFile(":/style/mywhitestyle.qss");
    styleFile.open(QFile::ReadOnly | QFile::Text);
    QTextStream ts(&styleFile);
    qApp->setStyleSheet(ts.readAll());
    styleFile.close();

    ui->action_light->setChecked(true);
    m_liveOverlay->setTheme(LiveOverlay::Theme::Light);
  }

  Language = settings.value("Language", "System").toString();
  bool loadOk = false;
  if (Language == "Chinese") {
    ui->actionChinese->setChecked(true);
    loadOk = mTranslator.load(":/i18n/zh_CN.qm");
  } else if (Language == "System") {
    ui->actionSystem->setChecked(true);
    if(QLocale::system().name() == "zh_CN")  { loadOk = mTranslator.load(":/i18n/zh_CN.qm"); }
    else { loadOk = mTranslator.load(":/i18n/qt_EN.qm"); }
  } else {
    ui->actionEnglish->setChecked(true);
    loadOk = mTranslator.load(":/i18n/qt_EN.qm");
  }

  if(loadOk) qApp->installTranslator(&mTranslator);
}

void MainWin::on_actionAbout_Qt_triggered() {
  QMessageBox::aboutQt(this, "About Qt");
}

void MainWin::on_action_light_triggered() {
    QString style = "Fusion";
    qApp->setStyle(QStyleFactory::create(style));
    QFile styleFile(":/style/mywhitestyle.qss");
    styleFile.open(QFile::ReadOnly | QFile::Text);
    QTextStream ts(&styleFile);
    qApp->setStyleSheet(ts.readAll());
    styleFile.close();

    ui->action_light->setChecked(true);
    m_liveOverlay->setTheme(LiveOverlay::Theme::Light);

    QString inifile = ConfigManager::configFilePath();
    QSettings setting(inifile, QSettings::IniFormat);
    setting.setValue("style", "light");
}

void MainWin::on_action_gray_triggered() {
  QString style = "Fusion";
  qApp->setStyle(QStyleFactory::create(style));
  QFile styleFile(":/style/mygraystyle.qss");
  styleFile.open(QFile::ReadOnly | QFile::Text);
  QTextStream ts(&styleFile);
  qApp->setStyleSheet(ts.readAll());
  styleFile.close();
  qApp->setFont(QFont("Arial", 9));

  ui->action_gray->setChecked(true);
  m_liveOverlay->setTheme(LiveOverlay::Theme::Gray);

  QString inifile = ConfigManager::configFilePath();
  QSettings setting(inifile, QSettings::IniFormat);
  setting.setValue("style", "gray");
}

void MainWin::on_action_dark_triggered() {
  QString style = "Fusion";
  qApp->setStyle(QStyleFactory::create(style));
  QFile styleFile(":/style/myblackstyle.qss");
  styleFile.open(QFile::ReadOnly | QFile::Text);
  QTextStream ts(&styleFile);
  qApp->setStyleSheet(ts.readAll());
  styleFile.close();
  qApp->setFont(QFont("Arial", 9));

  ui->action_dark->setChecked(true);
  m_liveOverlay->setTheme(LiveOverlay::Theme::Dark);

  QString inifile = ConfigManager::configFilePath();
  QSettings setting(inifile, QSettings::IniFormat);
  setting.setValue("style", "dark");
}

void MainWin::on_actionSystem_triggered() {
    QString inifile = ConfigManager::configFilePath();
    QSettings setting(inifile, QSettings::IniFormat);
    setting.setValue("Language", "System");

    ui->actionSystem->setChecked(true);
    if (QLocale::system().name() == "zh_CN") {
    if (mTranslator.load(":/i18n/zh_CN.qm")) { qApp->installTranslator(&mTranslator); }
    } else {
    if (mTranslator.load(":/i18n/qt_EN.qm")) { qApp->installTranslator(&mTranslator); }
    }
}

void MainWin::on_actionChinese_triggered() {
  QString inifile = ConfigManager::configFilePath();
  QSettings setting(inifile, QSettings::IniFormat);
  setting.setValue("Language", "Chinese");

  if (mTranslator.load(":/i18n/zh_CN.qm")) {
    qApp->installTranslator(&mTranslator);
    ui->actionChinese->setChecked(true);
  }
}

void MainWin::on_actionEnglish_triggered() {
  QString inifile = ConfigManager::configFilePath();
  QSettings setting(inifile, QSettings::IniFormat);
  setting.setValue("Language", "English");

  if (mTranslator.load(":/i18n/qt_EN.qm")) {
    qApp->installTranslator(&mTranslator);
    ui->actionEnglish->setChecked(true);
  }
}

void MainWin::on_actionExit_triggered() { qApp->exit(0); }

void MainWin::onHotkeyPressed() {
    LOG_DEBUG(QString("Activated:" + ui->shortcut_edit->getShortCut()));
    const bool idle = m_workflow->state() == WorkflowState::Idle;
    if (ConfigManager::instance().config().hotkeyHold) {
        // 按住说话：按下开始，松开时由 onHotkeyReleased 结束
        if (idle && startDictation()) {
            m_holdStarted = true;
            m_holdTimer.start();
        }
        return;
    }
    if (idle) startDictation();
    else stopDictation();
}

void MainWin::onHotkeyReleased() {
    if (!m_holdStarted) return;
    m_holdStarted = false;
    // 按得太短多半是误触，丢弃这段录音，不送去识别
    constexpr qint64 kMinHoldMs = 300;
    if (m_holdTimer.elapsed() < kMinHoldMs) {
        LOG_DEBUG("Hotkey hold too short, recording discarded");
        m_workflow->cancel();
        setDictationIndicator(false);
        return;
    }
    stopDictation();
}

void MainWin::setDictationIndicator(bool recording) {
    m_indicatorRecording = recording;
    ui->hotkey_status_dot->setListening(recording);
    ui->hotkey_status_text->setText(recording ? tr("Recording") : tr("Idle"));
}

bool MainWin::startDictation() {
    if (ui->file_transcribe_widget->isBusy()) {
        LOG_WARN("File transcription in progress, dictation hotkey ignored");
        m_liveOverlay->hideOverlay();
        return false;
    }
    const auto& config = ConfigManager::instance().config();

    // 只有本地识别才需要本地模型；云端后端不受影响
    if (config.backend == AsrBackendKind::Sherpa) {
        const QString repoId = ModelRegistry::FindByDisplayName(config.sherpa.languageModel,
                                                                config.sherpa.localModelRepoId);
        if (!m_sherpaInstaller->isInstalled(repoId)) {
            LOG_ERROR("Current model is not installed, please install it first");
            notify(NotifyLevel::Warning, tr("本地模型未安装"), tr("请先在\"本地识别\"页下载当前语言的模型"));
            m_liveOverlay->hideOverlay();
            return false;
        }
        if (m_sherpaInstaller->isInstalling(repoId)) {
            LOG_ERROR("Current model is being installed");
            notify(NotifyLevel::Info, tr("模型正在下载"), tr("下载完成后即可开始听写"));
            m_liveOverlay->hideOverlay();
            return false;
        }
    }

    m_workflow->start();
    setDictationIndicator(true);
    return true;
}

void MainWin::stopDictation() {
    m_workflow->stop();
    setDictationIndicator(false);
}

void MainWin::onStateChanged(WorkflowState state)
{
    setSettingsLocked(state != WorkflowState::Idle);

    switch (state) {
    case WorkflowState::Loading:
        m_captionModel->clear();
        syncLiveCaption(); 
        m_liveOverlay->setLoading();
        m_liveOverlay->showOnScreen();
        break;
    case WorkflowState::Recording:
        m_liveOverlay->setListening();
        m_liveOverlay->showOnScreen();
        if (ConfigManager::instance().config().continuousMode.load()
            && !m_recorderService->isVoiceActive()) {
            m_liveOverlay->hideTimerStart();
        }
        else {
            m_liveOverlay->hideTimerStop();   
        }
        break;
    case WorkflowState::Transcribing:
        m_liveOverlay->hideTimerStop();
        m_liveOverlay->setTranscribe();
        m_liveOverlay->showOnScreen();
        break;
    case WorkflowState::Processing:
        m_liveOverlay->hideTimerStart();
        break;
    case WorkflowState::Idle:
        // 有字幕时多留一会儿，让用户看清最终结果；期间若又开始了新一轮录音就不能再隐藏
        QTimer::singleShot(m_liveOverlay->hasCaption() ? 1800 : 200, this, [this]() {
            if (m_workflow->state() == WorkflowState::Idle) m_liveOverlay->hideOverlay();
        });
        break;
    case WorkflowState::Error:
        QTimer::singleShot(200, [this]() {
            m_liveOverlay->hideOverlay();
            });
        break;
    default:
        break;
    }
}

void MainWin::setSettingsLocked(bool locked)
{
    m_settings->setDictating(locked);
}

void MainWin::onToggleDrawer(int pageIndex) {
    ui->drawer_stack->setCurrentIndex(pageIndex);
    // if (isTriggered) {
    //     isTriggered = false;
    //     m_drawerAnimation->setStartValue(ui->drawer_stack->maximumHeight());
    //     m_drawerAnimation->setEndValue(0);
    // }
    // else {
    //     isTriggered = true;
    //     m_drawerAnimation->setStartValue(ui->drawer_stack->maximumHeight());
    //     m_drawerAnimation->setEndValue(200);
    // }
    // m_drawerAnimation->start();
}

void MainWin::onNewLogEntry(const QString& entry)
{
    if (ui && ui->config_log_edit) {
        QTextCursor cursor = ui->config_log_edit->textCursor();
        cursor.movePosition(QTextCursor::End);

        QTextCharFormat format;
        if (entry.contains("ERROR", Qt::CaseInsensitive) || entry.contains("[E]", Qt::CaseInsensitive)) {
            format.setForeground(QColor("#FF3333"));
            format.setFontWeight(QFont::Bold);
        }
        else if (entry.contains("WARN", Qt::CaseInsensitive) || entry.contains("[W]", Qt::CaseInsensitive)) {
            format.setForeground(QColor("#FFA500"));
            format.setFontWeight(QFont::Bold);
        }
        else if (entry.contains("DEBUG", Qt::CaseInsensitive) || entry.contains("[D]", Qt::CaseInsensitive)) {
            format.setForeground(QColor("#007799"));
            format.setFontWeight(QFont::Normal);
        }
        else if (entry.contains("INFO", Qt::CaseInsensitive) || entry.contains("[I]", Qt::CaseInsensitive)) {
            format.setForeground(QColor("#00A854"));
            format.setFontWeight(QFont::Normal);
        }
        else {
            format.setForeground(QColor("#333333"));
            format.setFontWeight(QFont::Normal);
        }

        cursor.setCharFormat(format);
        cursor.insertText(entry + "\n");
    }
}

void MainWin::notify(NotifyLevel level, const QString& titleCN, const QString& causeCN)
{
    QString text = titleCN;
    if (!causeCN.isEmpty()) text += QString("\n%1").arg(causeCN);
    switch (level) {
    case NotifyLevel::Info:
        InforBar::info("", text, InforBarPosition::I_BOTTOM_RIGHT, this);
        break;
    case NotifyLevel::Success:
        InforBar::success("", text, InforBarPosition::I_BOTTOM_RIGHT, this);
        break;
    case NotifyLevel::Warning:
        InforBar::warning("", text, InforBarPosition::I_BOTTOM_RIGHT, this);
        break;
    case NotifyLevel::Error:
        InforBar::error("", text, InforBarPosition::I_BOTTOM_RIGHT, this);
        break;
    }
}

void MainWin::onUpdateFound(const QString& version, const QString& downloadUrl, const QString& notes) {
    QMessageBox msgBox(this);
    msgBox.setWindowTitle(tr("Update Available"));
    msgBox.setText(tr("A new version (%1) is available!").arg(version));
    msgBox.setInformativeText(tr("Would you like to download and install it now?"));

    msgBox.setDetailedText(notes);
    msgBox.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
    msgBox.setDefaultButton(QMessageBox::Ok);
    msgBox.setIcon(QMessageBox::Information);

    // "不再提醒"复选项：勾选后持久化，后续启动不再弹更新提示
    QCheckBox* neverAgain = new QCheckBox(tr("Do not remind me again"));
    msgBox.setCheckBox(neverAgain);

    int ret = msgBox.exec();

    if (neverAgain->isChecked()) {
        ConfigManager::instance().config().skipUpdateReminder = true;
        ConfigManager::instance().save();
        LOG_INFO("User opted out of update reminders");
    }

    if (ret == QMessageBox::Ok) {
        QString ext;
#if defined(Q_OS_WIN)
        ext = ".exe";
#elif defined(Q_OS_MAC)
        ext = ".dmg";
#elif defined(Q_OS_LINUX)
        ext = ".AppImage";
#endif
        m_updateManager->downloadUpdate(QDir::tempPath() + "/VoiceIME_new_version" + ext);
        ui->actionUpdate->setEnabled(false);
        qDebug() << "Update download started...";
    }
}

void MainWin::onUpdateDownloaded(const QString& filePath) {
    auto result = QMessageBox::question(this, tr("Restart Required"),
        tr("Update downloaded successfully. The application will now close and restart to apply the update. Continue?"),
        QMessageBox::Yes | QMessageBox::No);

    if (result == QMessageBox::Yes) {
        QString err;
        if (m_updateManager->launchUpdaterAndPrepareExit(filePath, &err)) {
            qApp->exit();
        }
        else {
            notify(NotifyLevel::Error, tr("更新失败"), err);
            ui->actionUpdate->setEnabled(true);
        }
    }
    else {
        ui->actionUpdate->setEnabled(true);
    }
}

void MainWin::onLoadConfig()
{
    LOG_DEBUG("Load Configuration");
    loadConfigToUI();
    readIni();

    m_workflow->applyRecorderConfig();
    syncLiveCaption();
}

void MainWin::initSystemTray()
{
    // 菜单内容随状态变化
    trayMenu = new QMenu(this);
    connect(trayMenu, &QMenu::aboutToShow, this, &MainWin::rebuildTrayMenu);

    // 创建托盘图标
    trayIcon = new QSystemTrayIcon(this);
    trayIcon->setContextMenu(trayMenu);

    // 设置托盘图标
    QIcon icon;
    icon.addFile(QString::fromUtf8(":/title_icon"), QSize(), QIcon::Mode::Normal, QIcon::State::Off);
    if (icon.isNull()) {
        icon = QApplication::style()->standardIcon(QStyle::SP_ComputerIcon);
    }
    trayIcon->setIcon(icon);
    connect(trayIcon, &QSystemTrayIcon::activated, this, &MainWin::onTrayIconActivated);
    trayIcon->show();
}

void MainWin::rebuildTrayMenu()
{
    trayMenu->clear();
    const AppConfig& cfg = ConfigManager::instance().config();
    const bool idle = m_workflow->state() == WorkflowState::Idle;

    if (idle) trayMenu->addAction(tr("开始听写"), this, [this]() { startDictation(); });
    else trayMenu->addAction(tr("停止听写"), this, &MainWin::stopDictation);
    trayMenu->addSeparator();

    // 锁定状态，与设置页一致
    QMenu* langMenu = trayMenu->addMenu(tr("识别语言"));
    langMenu->setEnabled(idle);
    
    // 托盘里只列已下载模型的
    for (const QString& lang : ModelRegistry::GetLanguages()) {
        QString installedModel;
        for (const ModelEntry& e : ModelRegistry::GetModelEntriesByLanguage(lang)) {
            if (SherpaInstaller::isInstalled(e.repoPath)) { installedModel = e.displayName; break; }
        }
        if (installedModel.isEmpty() && lang != cfg.sherpa.languageModel) continue;
        QAction* a = langMenu->addAction(LanguageCatalog::displayName(lang), this, [this, lang, installedModel]() {
            ui->language_comb->setCurrentIndex(ui->language_comb->findData(lang));
            if (!installedModel.isEmpty()) ui->model_list_widget->setCurrentModel(installedModel);
            ConfigManager::instance().config().sherpa.localModelRepoId = ui->model_list_widget->currentModel();
            applySettings(SettingsBinder::LiveCaption);
            m_workflow->preloadModel(ConfigManager::instance().config());
        });
        a->setCheckable(true);
        a->setChecked(lang == cfg.sherpa.languageModel);
    }

    QMenu* backendMenu = trayMenu->addMenu(tr("识别后端"));
    backendMenu->setEnabled(idle);
    for (int i = 0; i < ui->identification_backend_comb->count(); ++i) {
        QAction* a = backendMenu->addAction(ui->identification_backend_comb->itemText(i), this, [this, i]() {
            ui->identification_backend_comb->setCurrentIndex(i); 
        });
        a->setCheckable(true);
        a->setChecked(i == int(cfg.backend));
    }

    QAction* caption = trayMenu->addAction(tr("实时字幕"), this, [this](bool on) {
        ui->live_caption_checkbox->setChecked(on); 
    });
    caption->setCheckable(true);
    caption->setChecked(cfg.liveCaptionEnabled.load());

    // 最近听写：点一下复制
    QMenu* recent = trayMenu->addMenu(tr("最近听写"));
    constexpr int kTrayRecent = 5;
    for (int i = 0; i < std::min(kTrayRecent, m_history->rowCount()); ++i) {
        const QString text = m_history->textAt(i);
        const QString label = QFontMetrics(recent->font()).elidedText(text.simplified(), Qt::ElideRight, 320);
        recent->addAction(label, this, [this, text]() {
            QGuiApplication::clipboard()->setText(text);
            trayIcon->showMessage(tr("已复制"), text.left(80), QSystemTrayIcon::Information, 1500);
        });
    }
    if (recent->isEmpty()) recent->addAction(tr("暂无记录"))->setEnabled(false);

    trayMenu->addSeparator();
    if (isVisible()) trayMenu->addAction(tr("隐藏主窗口"), this, &MainWin::onHideWindow);
    else trayMenu->addAction(tr("显示主窗口"), this, &MainWin::onShowWindow);
    trayMenu->addAction(tr("退出"), this, &MainWin::onQuitApplication);
}

void MainWin::onTrayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
        if (isVisible()) {
            onHideWindow();
        }
        else {
            onShowWindow();
        }
    }
}

void MainWin::onShowWindow()
{
    showNormal();       
    activateWindow();   
    raise();            
}

void MainWin::onHideWindow()
{
    hide();
    setWindowState(Qt::WindowMinimized);
}

void MainWin::onQuitApplication()
{
    if (trayIcon) {
        trayIcon->hide();
    }
    qApp->exit(0);
}


void MainWin::setupLanguageCombo()
{
    QComboBox* comb = ui->language_comb;
    for (const QString& key : ModelRegistry::GetLanguages()) {
        comb->addItem(LanguageCatalog::displayName(key), key);
    }
}

void MainWin::refreshLanguageComboTexts()
{
    QComboBox* comb = ui->language_comb;
    const QSignalBlocker blocker(comb);  
    for (int i = 0; i < comb->count(); ++i) {
        comb->setItemText(i, LanguageCatalog::displayName(comb->itemData(i).toString()));
    }
    comb->setEditText(comb->itemText(comb->currentIndex()));
}

QString MainWin::currentLanguageKey() const
{
    return ui->language_comb->currentData().toString();
}
