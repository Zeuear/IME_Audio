#include "MainWin.h"
#include "ui_MainWin.h"

#include <QMenu>
#include <QToolButton>
#include <QClipboard>
#include <algorithm>
#include <QDebug>
#include <QLocale>
#include <QSet>
#include <QStandardItemModel>
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

#include "config/ConfigManager.h"
#include "config/AppConfig.h"
#include "services/WorkflowManager.h"
#include "services/RecorderService.h"
#include "services/TranscriptionService.h"
#include "services/TextPolishService.h"
#include "services/TranslationService.h"
#include "services/UpdateManager.h"
#include "services/TermsLibraryManager.h"
#include "services/ForegroundWindow.h"
#include "services/docs/DocsBridge.h"
#include "ui/controls/DocsSetupDialog.h"
#include "snippets/SnippetStore.h"
#include "services/PrefixWatcher.h"
#include "services/SnippetPaster.h"
#include "translation/BergamotWorkerClient.h"
#include "installers/TranslationModelInstaller.h"
#include "installers/TranslationModelStore.h"
#include "translation/LanguageRouting.h"
#include "utils/Logger.h"
#include "utils/AppPaths.h"
#include "sherpa/SherpaManager.h"
#include "sherpa/GpuRuntime.h"
#include "installers/CudaInstaller.h"
#include "sherpa/LanguageCatalog.h"
#include "sherpa/StreamingEngine.h"
#include "ui/controls/LiveOverlay.h"
#include "ui/controls/ControlBar.h"
#include "ui/controls/ToggleLink.h"
#include "ui/controls/Appearance.h"
#include "ui/controls/FirewallCard.h"
#include "ui/controls/SnippetSuggestBar.h"
#include "ui/controls/SnippetSearchPopup.h"
#include "ui/pages/SnippetWidget.h"
#include "ui/pages/FileTranscribeWidget.h"
#include "ui/pages/TermWidget.h"
#include "ui/pages/AboutPage.h"
#include "ui/pages/LiveTranscriptWidget.h"
#include "ui/models/DictationHistoryModel.h"
#include "ui/models/ModelDownloadTracker.h"
#include "ui/models/LiveCaptionModel.h"
#include "services/ollama/OllamaClient.h"
#include "services/textpolish/AiStagePlan.h"
#include "services/ollama/OllamaCatalog.h"
#include "ui/controls/OllamaModelButton.h"
#include "ui/controls/SettingsBinder.h"
#include "ui/controls/ExpandPanel.h"
#include "ui/controls/FloatingWindow.h"
#include "ui/controls/ToolIcon.h"
#include "ui/controls/AnimatedCheckbox.h"
#include "ui/infobar/inforbarmanager.h"
#include "ui/infobar/inforposmanager.h"
#include "ui/infobar/inforbar.h"

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

namespace {
void alignColumns(const QList<QGridLayout*>& grids, const QList<int>& cols)
{
    for (int col : cols) {
        int width = 0;
        for (QGridLayout* grid : grids) {
            for (int i = 0; i < grid->count(); ++i) {
                int r, c, rowSpan, colSpan;
                grid->getItemPosition(i, &r, &c, &rowSpan, &colSpan);
                if (c == col && colSpan == 1 && grid->itemAt(i)->widget() && !grid->itemAt(i)->widget()->isHidden())
                    width = std::max(width, grid->itemAt(i)->widget()->sizeHint().width());
            }
        }
        for (QGridLayout* grid : grids) grid->setColumnMinimumWidth(col, width);
    }
}
} // namespace

void MainWin::alignSettingsCards()
{
    alignColumns({ ui->device_card_layout, ui->recording_card_layout, ui->live_caption_card_layout,
                   ui->shortcut_card_layout }, { 0, 3 });
    // 第 2 列是左右两组之间的空隔
    for (QGridLayout* grid : { ui->device_card_layout, ui->recording_card_layout, ui->live_caption_card_layout,
                               ui->shortcut_card_layout })
        grid->setColumnMinimumWidth(2, 24);

    alignColumns({ ui->ai_service_card_layout, ui->polish_card_layout, ui->translate_card_layout }, { 0, 2 });
}

void MainWin::initialize() {
    ui->recording_equ_comb->clear();
    auto items = AudioRecorderService::availableMicrophones();
    ui->recording_equ_comb->addItems(items);

    setupLanguageCombo();
    ui->model_list_widget->setLanguage(currentLanguageKey());

    setupAppearanceUi();
    retranslateDynamicItems();

    m_networkManager = new QNetworkAccessManager(this);

    const AppConfig& config = ConfigManager::instance().config();
    m_textPolishService = new TextPolishService(m_networkManager, this);
    m_ollamaClient = new OllamaClient(m_networkManager, this);
    m_ollamaClient->setBaseUrl(config.polish.apiUrl);
    {
        QFile manifestFile(QStringLiteral(":/translation_models.json"));
        manifestFile.open(QIODevice::ReadOnly);
        m_translationInstaller = new TranslationModelInstaller(
            m_networkManager, AppPaths::translationModelsDir(),
            TranslationModelStore::parseBundledManifest(manifestFile.readAll()), this);
    }
    // worker 由构建时下载并放在主程序同级目录（macOS 为 Contents/MacOS）；不存在时该项灰显，翻译管线直接跳过
#ifdef Q_OS_WIN
    const QString workerName = QStringLiteral("translate_worker.exe");
#else
    const QString workerName = QStringLiteral("translate_worker");
#endif
    TranslationModelInstaller* installer = m_translationInstaller;
    ITranslationBackend* translationBackend = new BergamotWorkerClient(
        BergamotWorkerClient::RouteProvider{
            [installer](const QString& from, const QString& to) { return planRoute(from, to, installer->knownPairs()); },
            [installer](const QString& from, const QString& to) { return installer->isInstalled(from, to); },
            [installer](const QString& from, const QString& to) { return installer->writeConfigFile(from, to); } },
        QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(workerName), this);


    m_translationService = new TranslationService(translationBackend, this);
    m_recorderService = new AudioRecorderService(config, this);
    m_sherpaManager = new SherpaManager(this);
    m_sherpaInstaller = new SherpaInstaller(m_networkManager, this);
    m_cudaInstaller = new CudaInstaller(m_networkManager, m_sherpaInstaller, this);
    m_gpuRuntime = new GpuRuntime(this);
    m_gpuRuntime->setInstaller(m_cudaInstaller);
    m_gpuRuntime->setDetector([] { return CudaInstaller::detectGpuEnvironment(true); });

    m_transcriptionService = new TranscriptionService(m_networkManager, m_sherpaManager, m_textPolishService,
                                                      m_translationService, config, this);
    m_termsManager = new TermsLibraryManager(this);
    m_updateManager = new UpdateManager(m_networkManager, this);
    ui->about_page->setUpdateManager(m_updateManager);

    m_workflow = new WorkflowManager(config, this);
    m_workflow->initialize(m_recorderService, m_transcriptionService, m_sherpaManager);

    // 初始化动画

    // 声音控件
    m_liveOverlay = new LiveOverlay();
    m_liveOverlay->loadSavedPosition();
    // 没有托盘的桌面隐藏后没有地方能再叫出来，那就不给隐藏按钮
    m_liveOverlay->setHideButtonEnabled(QSystemTrayIcon::isSystemTrayAvailable());
    connect(m_liveOverlay, &LiveOverlay::hiddenByUser, this, [this]() {
        notify(NotifyLevel::Info, QStringLiteral("语音识别窗口已隐藏"), QStringLiteral("可在系统托盘图标的右键菜单中重新显示"));
    });
    m_controlBar = new ControlBar();
    m_transcriptWidget = new LiveTranscriptWidget();
    m_termWidget = new TermWidget();
    m_fileWidget = new FileTranscribeWidget();
    m_panel = new ExpandPanel();
    m_panel->setPages(m_transcriptWidget, m_termWidget, m_fileWidget);   // 页面归面板所有
    m_formSwitchBtn = new QToolButton(ui->tabWidget);   // Designer 不能设置标签栏角落控件，只能在代码里加
    m_formSwitchBtn->setAutoRaise(true);
    m_formSwitchBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_formSwitchBtn->setIconSize(QSize(22, 22));
    m_formSwitchBtn->setCursor(Qt::PointingHandCursor);
    m_formSwitchBtn->setText(tr("Compact mode"));
    m_formSwitchBtn->setToolTip(tr("Switch to the compact toolbar"));
    refreshFormSwitchIcon();
    ui->tabWidget->setCornerWidget(m_formSwitchBtn, Qt::TopRightCorner);
    connect(m_formSwitchBtn, &QToolButton::clicked, this, &MainWin::switchToBar);
    m_panel->attachTo(m_controlBar);                 // 面板紧贴横条，横条拖到哪它跟到哪
    m_captionModel = new LiveCaptionModel(this);
    m_history = new DictationHistoryModel(this);
    m_captionEngine = new StreamingEngine();
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

    // VAD 模型随安装包分发；若缺失(旧安装包/被误删)则自动补下载，避免用户卡在
    // "VAD 模型缺失，请先下载模型" 提示上却无从下手。
    m_sherpaInstaller->ensureVadModel();
    ui->gpu_backend_widget->setGpuRuntime(m_gpuRuntime);
    m_termWidget->setTermsManager(m_termsManager);
    m_snippetStore = new SnippetStore(AppPaths::snippetsFile());
    if (!m_snippetStore->load()) {
        LOG_WARN("Snippet library is damaged, falling back to defaults in memory");
    }
    m_snippetPaster = new SnippetPaster(m_snippetStore, m_networkManager, this);
    m_suggestBar = new SnippetSuggestBar();
    m_searchPopup = new SnippetSearchPopup(m_snippetStore, m_snippetPaster);
    m_prefixWatcher = new PrefixWatcher(this);
    ui->snippet_widget->setSnippetStore(m_snippetStore);
    m_fileWidget->setServices(m_networkManager, m_transcriptionService, m_sherpaManager,
                              &ConfigManager::instance().config());
    m_transcriptWidget->setSessionFile(AppPaths::transcriptFile());
}

void MainWin::setupUiConnections(){
    auto& configManager = ConfigManager::instance();

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

    // 模型行内的下载进度：安装器已有的信号喂给 tracker，列表据此画进度条；行里的"取消 / 重试"回到安装器
    m_downloadTracker = new ModelDownloadTracker(this);
    ui->model_list_widget->setDownloadTracker(m_downloadTracker);
    connect(m_sherpaInstaller, &SherpaInstaller::installGroupStarted, m_downloadTracker, &ModelDownloadTracker::onGroupStarted);
    connect(m_sherpaInstaller, &SherpaInstaller::installFileProgress, m_downloadTracker, &ModelDownloadTracker::onFileProgress);
    connect(m_sherpaInstaller, &SherpaInstaller::installFileFinished, m_downloadTracker, &ModelDownloadTracker::onFileFinished);
    connect(m_sherpaInstaller, &SherpaInstaller::installFileError, m_downloadTracker, &ModelDownloadTracker::onFileError);
    connect(m_sherpaInstaller, &SherpaInstaller::extractStarted, m_downloadTracker, &ModelDownloadTracker::onExtractStarted);
    connect(m_sherpaInstaller, &SherpaInstaller::extractProgress, m_downloadTracker, &ModelDownloadTracker::onExtractProgress);
    connect(m_sherpaInstaller, &SherpaInstaller::installGroupFinished, m_downloadTracker, &ModelDownloadTracker::onGroupFinished);
    connect(m_sherpaInstaller, &SherpaInstaller::installCancelled, m_downloadTracker, &ModelDownloadTracker::markCancelled);
    connect(ui->model_list_widget, &ModelListWidget::cancelRequested, m_sherpaInstaller, &SherpaInstaller::cancelInstall);
    connect(ui->model_list_widget, &ModelListWidget::retryRequested, m_sherpaInstaller, &SherpaInstaller::installModel);

    connect(m_sherpaInstaller, &SherpaInstaller::installGroupStarted, this, [=]() {
        notify(NotifyLevel::Info, tr("开始下载模型..."));
    });
    connect(m_sherpaInstaller, &SherpaInstaller::installCancelled, this, [this](const QString&) { m_userCancelledInstall = true; });
    connect(m_sherpaInstaller, &SherpaInstaller::extractFailed, this, [this](const QString& repoId, const QString& cause) {
        m_installFailCause.insert(repoId, cause);
    });
    connect(m_sherpaInstaller, &SherpaInstaller::installGroupFinished, this, [=](const QString& repoId, bool success, const QString&) {
        if (repoId == m_autoGpuRepoId) m_autoGpuRepoId.clear();
        ui->model_list_widget->refreshInstalled();
        const QString extractCause = m_installFailCause.take(repoId);
        if (success) notify(NotifyLevel::Success, tr("模型下载完成"));
        else if (m_userCancelledInstall) notify(NotifyLevel::Info, tr("已取消下载"));   // 取消不是失败，不弹错误
        else if (!extractCause.isEmpty()) notify(NotifyLevel::Error, tr("模型解压失败"), extractCause);
        else notify(NotifyLevel::Error, tr("模型下载失败"), tr("请检查网络后重试"));
        m_userCancelledInstall = false;
    });
    connect(m_sherpaInstaller, &SherpaInstaller::vadModelReady, this, [this](bool success, const QString&) {
        if (success) {
            LOG_INFO("VAD model ready, rebuilding VAD detector");
            m_workflow->applyRecorderConfig();
        } else {
            notify(NotifyLevel::Error, tr("VAD 模型下载失败"), tr("请检查网络后重试，或前往 \"下载列表\" 查看进度"));
        }
    });
    connect(m_sherpaManager, &SherpaManager::loadFailed, this, [this](const QString& title, const QString& cause) {
        notify(NotifyLevel::Error, title, cause);
    });
    qRegisterMetaType<GpuLoadResult>();
    connect(m_sherpaManager, &SherpaManager::loadProviderResolved, this,
            [this](const GpuLoadResult& result) {
                m_gpuRuntime->reportLoadResult(result);
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
        // 检测结果可能把模式改回 CPU 而不发 computeModeChanged，列表的 GPU 提示要跟上
        ui->model_list_widget->setGpuMode(result);
        onGpuDetectionFinished(result);
    });

    // gemini
    connect(ui->check_connection_btn, &QPushButton::clicked, this, [this]() {
        const int engine = ui->ai_engine_comb->currentIndex();
        // Ollama 下同时重新检测服务：装好 Ollama 后点这里即可
        if (engine == 2) m_ollamaClient->checkService();
        if (!m_textPolishService->capabilities(engine).canTest) return;

        const AppConfig& config = ConfigManager::instance().config();
        TextPolishService::RequestParams params;
        params.aiEngine = engine;
        // 地址、密钥直接取输入框当前内容：它们是"提交时才写进配置"，刚输入完就点测试时配置里还是旧值
        params.apiKey = ui->api_key_edit->isVisible() ? ui->api_key_edit->text() : config.polish.apiKey;
        params.customUrl = ui->api_url_edit->text();
        params.model = config.polish.model;
        params.style = config.polish.aiStyle;
        params.customPrompt = config.polish.prompt;
        params.aiVocab = config.polish.vocab;
        params.targetLang = config.polish.cloudTargetLang();
        m_textPolishService->testConnection(params);
    });

    // 测试结果必须让用户看到：以前只写了日志，点了"测试连接"界面上没有任何反应
    connect(m_textPolishService, &TextPolishService::connectionTested, this, [this](bool ok, const QString& message) {
        notify(ok ? NotifyLevel::Success : NotifyLevel::Error, ok ? QStringLiteral("连接成功") : QStringLiteral("连接失败"), message);
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
    connect(m_translationInstaller, &TranslationModelInstaller::installGroupStarted, this,
            [this](const QString&, const QString&, int totalFiles) {
                m_translateProgress.start(totalFiles);
                ui->translate_progress_bar->setValue(0);
                updateTranslationUi();
            });
    connect(m_translationInstaller, &TranslationModelInstaller::installFileProgress, this,
            [this](const QString&, const QString& filename, qint64, qint64, int overallPercent) {
                m_translateProgress.update(filename, overallPercent);
                ui->translate_progress_bar->setValue(m_translateProgress.percent());
                updateTranslationUi();
            });
    connect(m_translationInstaller, &TranslationModelInstaller::installFileFinished, this,
            [this]() { m_translateProgress.fileFinished(); });
    connect(m_translationInstaller, &TranslationModelInstaller::installGroupFinished, this, [this]() { updateTranslationUi(); });

    connect(ui->ai_engine_comb, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        // 目标语言列表随引擎变（云端几种常用语言 / 本地按已有模型）；载入设置时由 loadConfigToUI 统一重建
        if (!m_loadingSettings) rebuildTranslationLanguages();
        if (!m_loadingSettings) swapPolishModelForEngine(index);
        updateAiEngineUi();
        // 载入设置时由 loadConfigToUI 在末尾统一拉取，避免重复请求
        if (m_loadingSettings) return;
        if (index == 2) {
            m_ollamaClient->setBaseUrl(ConfigManager::instance().config().polish.apiUrl);
            m_ollamaClient->checkService();
        }
        requestModelList();
    });

    connect(m_textPolishService, &TextPolishService::modelsFetched, this, [this](bool success, const QStringList& models, const QString&) {
        const bool ollama = ui->ai_engine_comb->currentIndex() == 2;
        const bool listed = success && !models.isEmpty();
        if (!listed) {
            LOG_ERROR("Failed to fetch model list");
            // Ollama 没开时仍列出推荐模型和已保存的模型，旁边的按钮提示安装 / 下载
            if (!ollama) {
                ui->ai_model_comb->clear();
                rebuildTranslateModelCombo({});
                return;
            }
        } else {
            LOG_INFO("Fetched model list successfully");
        }
        if (ollama && success) {
            ui->ollama_model_btn->setInstalled(models);
            ui->ollama_translate_btn->setInstalled(models);
        }
        const QStringList installed = success ? models : QStringList();
        rebuildTranslateModelCombo(installed);
        rebuildPolishModelCombo(installed, listed);
    });

    // 更新检查
    connect(m_updateManager, &UpdateManager::updateAvailable, this, &MainWin::onUpdateFound);
    connect(m_updateManager, &UpdateManager::downloadFinished, this, &MainWin::onUpdateDownloaded);
    connect(m_updateManager, &UpdateManager::updateError, this, [this](const QString& message) { LOG_ERROR(message); });

    // 日志输出
    connect(&Logger::instance(), &Logger::newLogEntry, this, &MainWin::onNewLogEntry, Qt::QueuedConnection);

    // 主要工作流
    connect(m_workflow, &WorkflowManager::transcriptionResultReady, m_history, [this](const QString& text) { m_history->add(text); });

    // 实时转录页：所有听写结果连续写成稿子；"同时输入到前台"关掉时结果只留在稿子里
    // 要翻译的句子：识别出来先显示原文，译文好了再补到同一行（翻译慢也不用干等）。
    // 同一句的 utteranceTranslated 在 transcriptionResultReady 之前同步发出：先把这句补完整并记下最终文字，
    // 结果到了发现已经显示过就不再另起一行；没经过翻译的句子仍按原来的方式直接追加
    connect(m_workflow, &WorkflowManager::utterancePending, this,
            [this](quint64 seq, const QString& source) { m_transcriptWidget->appendPending(seq, source); });
    connect(m_workflow, &WorkflowManager::utteranceTranslated, this,
            [this](quint64 seq, const QString& source, const QString& finalText) {
                m_transcriptWidget->completePending(seq, source, finalText);
                m_liveShownText = finalText;
                m_liveShownValid = true;
            });
    connect(m_workflow, &WorkflowManager::transcriptionResultReady,
            m_transcriptWidget, [this](const QString& text) {
                const bool alreadyShown = m_liveShownValid && m_liveShownText == text;
                m_liveShownValid = false;
                m_liveShownText.clear();
                if (!alreadyShown) m_transcriptWidget->appendUtterance(text);
            });

    connect(m_transcriptWidget, &LiveTranscriptWidget::startRequested, this, [this]() { startDictation(); });
    connect(m_transcriptWidget, &LiveTranscriptWidget::stopRequested, this, [this]() { stopDictation(); });
    connect(m_transcriptWidget, &LiveTranscriptWidget::copied, this,
            [this](const QString& title) { notify(NotifyLevel::Success, title); });
    connect(m_transcriptWidget, &LiveTranscriptWidget::errorOccurred, this,
            [this](const QString& title, const QString& cause) { notify(NotifyLevel::Error, title, cause); });
    // 只通知工作流，不写配置：持久化归设置页复选框上的绑定器（唯一来源）。
    // 这里曾经自己 save()，而启动时 ToggleLink 的初始同步也会走到这里——此刻配置还没载入，
    // 内存里是默认值，一保存就把用户的快捷键、API 密钥等整份覆盖掉
    connect(m_transcriptWidget, &LiveTranscriptWidget::injectToggled, this,
            [this](bool on) { m_workflow->setInjectEnabled(on); });
    m_workflow->setInjectEnabled(m_transcriptWidget->injectToForeground());

    // 横条上的三个开关不自己存状态：直接连到设置页里已有的复选框，
    // 点图标就是点那个复选框，已有的配置绑定、保存和界面联动照常触发
    ToggleLink::connect(m_controlBar->toggleButton(ControlBar::Toggle::Enhance), ui->enable_enhancement_checkbox);
    ToggleLink::connect(m_controlBar->toggleButton(ControlBar::Toggle::Translate), ui->translate_enabled_checkbox);
    // 「输入到前台窗口」以设置卡片里的复选框为准；稿子页里那个复选框也只是遥控器
    ToggleLink::connect(m_controlBar->toggleButton(ControlBar::Toggle::Inject), ui->inject_checkbox);
    ToggleLink::connect(m_transcriptWidget->injectCheckBox(), ui->inject_checkbox);

    // 横条：开始 / 停止和快捷键、稿子页走同一条入口，状态由 onStateChanged 同步
    connect(m_controlBar, &ControlBar::startRequested, this, [this]() { startDictation(); });
    connect(m_controlBar, &ControlBar::stopRequested, this, [this]() { stopDictation(); });
    connect(m_controlBar, &ControlBar::settingsRequested, this, &MainWin::switchToSettings);
    // 展开面板：图标只"请求"，面板变成什么样由 PanelState 决定；面板变了（含自己点 tab、被隐藏）再点亮图标
    connect(m_controlBar, &ControlBar::panelRequested, this, [this](PanelState::Page clicked) {
        m_panel->showPage(PanelState::afterClick(m_panel->page(), clicked));
    });
    connect(m_panel, &ExpandPanel::pageChanged, m_controlBar, &ControlBar::setPanelPage);
    connect(m_fileWidget, &FileTranscribeWidget::busyChanged, m_controlBar, &ControlBar::setFileBusy);
    connect(m_controlBar, &ControlBar::quitRequested, this, &MainWin::onQuitApplication);
    connect(m_controlBar, &ControlBar::hideRequested, this, &MainWin::hideControlBar);
    // 没有托盘的桌面隐藏后就找不回来了，此时不提供隐藏
    m_controlBar->hideAction()->setEnabled(QSystemTrayIcon::isSystemTrayAvailable());
    m_docsBridge = new DocsBridge(this);
    connect(m_docsBridge, &DocsBridge::insertFailed, this, [this](const QString& reason) {
        notify(NotifyLevel::Warning, QStringLiteral("写入 Google 文档失败"),
               QStringLiteral("没写进去的句子已暂存，重新绑定文档后会自动补写。原因：%1").arg(reason));
    });
    connect(m_controlBar, &ControlBar::docsRequested, this, [this]() {
        if (!m_docsDialog) {
            m_docsDialog = new DocsSetupDialog(this);
            m_docsDialog->setBridge(m_docsBridge);
            connect(m_docsDialog, &DocsSetupDialog::errorOccurred, this,
                    [this](const QString& title, const QString& cause) { notify(NotifyLevel::Error, title, cause); });
        }
        m_docsDialog->show();
        m_docsDialog->raise();
        m_docsDialog->activateWindow();
    });
    // 没导出过扩展就不开端口
    if (DocsBridge::hasToken()) m_docsBridge->setEnabled(true);
    connect(m_controlBar, &ControlBar::alwaysOnTopToggled, this, [this](bool on) {
        ConfigManager::instance().config().barAlwaysOnTop = on;
        ConfigManager::instance().save();
        m_panel->setAlwaysOnTop(on);
    });
    showCurrentForm();

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
    connect(m_recorderService, &AudioRecorderService::audioChunk, m_captionEngine, &StreamingEngine::acceptAudio);
    connect(m_captionEngine, &StreamingEngine::partialText, m_captionModel, &LiveCaptionModel::setPartial);
    // 强制切段时切点在过去，换新流要把切点之后已经喂给旧流的音频补喂回去，否则下一句字幕开头会丢
    connect(m_captionModel, &LiveCaptionModel::epochChanged, m_captionEngine, [this](quint64 epoch) {
        const int replayMs = m_recorderService->takeCarryMs();
        QMetaObject::invokeMethod(m_captionEngine, [engine = m_captionEngine, epoch, replayMs]() {
            engine->resetStream(epoch, replayMs);
        }, Qt::QueuedConnection);
    });
    connect(m_captionEngine, &StreamingEngine::modelReady, this, [this](const QString&, bool ok) {
        m_recorderService->setAudioTapEnabled(ok && ConfigManager::instance().config().liveCaptionEnabled.load());
        // 每次开始录音都会重试装载，只提示一次，免得每次录音都弹窗
        if (!ok && !m_liveCaptionFailNotified) {
            m_liveCaptionFailNotified = true;
            notify(NotifyLevel::Warning, tr("实时字幕不可用"), tr("流式识别模型加载或解码失败，听写不受影响"));
        }
    });
    connect(m_recorderService, &AudioRecorderService::voiceStarted, m_captionModel, [this]() { m_captionModel->setSpeaking(true); });
    connect(m_recorderService, &AudioRecorderService::voiceStopped, m_captionModel, [this]() { m_captionModel->setSpeaking(false); });
    // 切出一句后，用字幕已识别出的这句话按读音挑词典里的热词；utteranceAccepted 先于 transcribe 触发，这时这句已是 Pending
    m_transcriptionService->setHotwordHintProvider([this]() { return m_captionModel->pendingText(); });
    connect(m_workflow, &WorkflowManager::utteranceAccepted, m_captionModel, [this](quint64 id) { m_captionModel->onUtteranceReady(id); });
    connect(m_workflow, &WorkflowManager::utteranceFinished, m_captionModel,
            [this](const QString& text, quint64 id) { m_captionModel->onTranscribed(id, text); });
    connect(m_sherpaInstaller, &SherpaInstaller::streamingModelReady, this, &MainWin::syncLiveCaption);
    connect(m_sherpaManager, &SherpaManager::idleUnloaded, this, [this]() {
        m_recorderService->setAudioTapEnabled(false);
        QMetaObject::invokeMethod(m_captionEngine, &StreamingEngine::unloadModel, Qt::QueuedConnection);
    });

    // Sherpa 模型安装器：安装成功后经门面预热模型（不再直戳 m_sherpaManager）
    connect(m_sherpaInstaller, &SherpaInstaller::loadModel,
        this, [this](const QString& repoId, bool success, const QString& msg) {
            if (success && !repoId.isEmpty()) {
                requestPreload();
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
        m_transcriptionService->setHotwordEntries(rules.hotwordEntries);
		ConfigManager::instance().save();

        ui->ai_vocabulary_edit->setText(appConfig.polish.vocab);
    });
    // 词典在构造时就读完了，termsReloaded 当时没人接；启动后要补一次，否则改过词典前热词挑选是空的
    m_transcriptionService->setHotwordEntries(m_termsManager->buildAllRules().hotwordEntries);

    connect(ui->firewall_card, &FirewallCard::errorOccurred, this, [this](const QString& title, const QString& cause) {
        notify(NotifyLevel::Error, title, cause);
    });

    // Ollama：模型仍在下拉框里选，下拉框旁的按钮负责下载所选模型；两个按钮共用一个 client
    for (OllamaModelButton* btn : { ui->ollama_model_btn, ui->ollama_translate_btn }) {
        btn->setClient(m_ollamaClient);
        connect(btn, &OllamaModelButton::errorOccurred, this, [this](const QString& title, const QString& cause) {
            notify(NotifyLevel::Error, title, cause);
        });
        // 下载完成后重新拉模型列表，下拉框里的"未下载"标记随之消失
        connect(btn, &OllamaModelButton::modelInstalled, this, [this](const QString&) { requestModelList(); });
    }
    connect(m_ollamaClient, &OllamaClient::serviceChecked, this, [this](bool running, const QString&) {
        if (running && ui->ai_engine_comb->currentIndex() == 2) requestModelList();
    });
    connect(ui->ai_model_comb, &QComboBox::currentTextChanged, ui->ollama_model_btn, &OllamaModelButton::setModel);
    connect(ui->api_url_edit, &QLineEdit::editingFinished, this, [this]() {
        m_ollamaClient->setBaseUrl(ui->api_url_edit->text());
        if (ui->ai_engine_comb->currentIndex() == 2) m_ollamaClient->checkService();
    });
    connect(ui->translate_model_comb, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        if (m_loadingSettings || index < 0) return;
        AppConfig& cfg = ConfigManager::instance().config();
        const QString value = ui->translate_model_comb->itemData(index).toString();
        ui->ollama_translate_btn->setModel(value);
        if (cfg.polish.translateModel == value) return;
        cfg.polish.translateModel = value;
        applySettings(0);
        updateAiEngineUi();
    });
    connect(m_termWidget, &TermWidget::errorOccurred, this, [this](const QString& title, const QString& cause) {
        NotifyLevel level = title.contains(tr("成功")) ? NotifyLevel::Success
                            : title.contains(tr("冲突")) ? NotifyLevel::Warning
                            : NotifyLevel::Error;
        notify(level, title, cause);
    });

    // 指令设置：说到话术的快捷码 → 悬浮窗旁出现按钮 → 点击粘贴到前台窗口
    connect(ui->snippet_widget, &SnippetWidget::errorOccurred, this, [this](const QString& title, const QString& cause) {
        notify(title.contains(tr("成功")) ? NotifyLevel::Success : NotifyLevel::Error, title, cause);
    });
    connect(m_snippetPaster, &SnippetPaster::errorOccurred, this,
            [this](const QString& title, const QString& cause) { notify(NotifyLevel::Error, title, cause); });
    connect(m_suggestBar, &SnippetSuggestBar::snippetClicked, m_snippetPaster, &SnippetPaster::paste);
    connect(m_suggestBar, &SnippetSuggestBar::visibilityChanged, m_liveOverlay, &LiveOverlay::setHideHeld);

    m_workflow->setInjectFilter([this](const QString& text) {
        // 面板在前台时不注入
        if (m_panel && m_panel->isActiveWindow()) return true;

        // 输入到谷歌文档中
        // 绑定后连接意外断开也不注入前台，否则文字会打进别的软件；用户在横条菜单停止输出后才恢复
        if (m_docsBridge->holdsOutput()) {
            m_docsBridge->sendText(text);
            if (!m_docsBridge->isBound() && (!m_docsWarnTimer.isValid() || m_docsWarnTimer.elapsed() > 10000)) {
                m_docsWarnTimer.start();
                notify(NotifyLevel::Warning, QStringLiteral("Google 文档连接已断开"),
                       QStringLiteral("听写结果已暂存，重新绑定文档后会自动补写进去；不想等可以在横条右键菜单打开“Google 文档”点“停止输出”"));
            }
            return true;
        }
        if (!m_snippetStore->settings().voiceTrigger || !m_liveOverlay->isVisible()) return false;
        for (const SnippetItem* item : m_snippetStore->matchKeywords(text)) {
            if (!m_handledSnippetIds.contains(item->id) && SnippetSearchIndex::isExactKeyword(text, item->keyword))
                return true;
        }
        return false;
    });
    connect(ui->snippet_widget, &SnippetWidget::storeChanged, m_suggestBar, &SnippetSuggestBar::dismiss);
    connect(ui->snippet_widget, &SnippetWidget::searchPopupRequested, m_searchPopup, &SnippetSearchPopup::showPopup);

    // 打字前缀：勾选后才安装全局键盘钩子，取消勾选立即卸载
    auto applyPrefixTrigger = [this]() {
        const AppSettings& s = m_snippetStore->settings();
        m_prefixWatcher->setPrefixes(s.triggerPrefixes);
        m_prefixWatcher->setEnabled(s.enablePrefixTrigger);
    };
    applyPrefixTrigger();
    connect(ui->snippet_widget, &SnippetWidget::prefixTriggerChanged, this, applyPrefixTrigger);
    connect(m_prefixWatcher, &PrefixWatcher::prefixTyped, this, [this](const QString& prefix) {
        // 在本软件自己的窗口里打字（比如搜索框）不呼出，否则弹窗会盖在自己上面
        if (QApplication::activeWindow()) {
            LOG_DEBUG(QString("Prefix %1 ignored: typed inside the app window").arg(prefix));
            m_prefixWatcher->endQuery();
            return;
        }
        const bool erase = m_snippetStore->settings().autoDeletePrefix;
        m_searchPopup->showPopupAfterPrefix(erase ? static_cast<int>(prefix.size()) : 0);
    });
    // 弹窗不拿焦点，搜索词和导航键都由全局钩子转发
    connect(m_prefixWatcher, &PrefixWatcher::queryChanged, m_searchPopup, &SnippetSearchPopup::setQuery);
    connect(m_prefixWatcher, &PrefixWatcher::navKey, m_searchPopup, &SnippetSearchPopup::navigate);
    connect(m_searchPopup, &SnippetSearchPopup::closed, m_prefixWatcher, [this]() { m_prefixWatcher->endQuery(); });
    connect(m_suggestBar, &SnippetSuggestBar::closed, this,
            [this](const QStringList& ids) { m_handledSnippetIds.unite(QSet<QString>(ids.begin(), ids.end())); });
    connect(m_captionModel, &LiveCaptionModel::changed, this, [this]() {
        const QString text = m_captionModel->plainText();
        if (text.isEmpty()) m_handledSnippetIds.clear();   // 字幕被清空 = 下一句开始
        updateSnippetSuggestions(text);
    });
    connect(m_workflow, &WorkflowManager::transcriptionResultReady, this,
            [this](const QString& text) { updateSnippetSuggestions(text); });

    connect(m_fileWidget, &FileTranscribeWidget::errorOccurred, this,
            [this](const QString& title, const QString& cause) { notify(NotifyLevel::Error, title, cause); });

}

MainWin::~MainWin() {
  delete m_captionEngine;
  m_captionEngine = nullptr;
  delete m_suggestBar;
  delete m_searchPopup;
  delete m_panel;              // 连同稿子页、词典页和文件转录页；稿子控件析构时会把还没写盘的内容写掉
  delete m_controlBar;         // 面板是横条的子窗口，必须先删面板
  delete m_snippetStore;
  ConfigManager::instance().save();
  delete ui;
}

// 听写文本里出现话术快捷码时，在悬浮窗旁列出可点击的条目。
// 没命中时不主动收起：字幕被下一句清掉后，用户可能正要点按钮，由指令条自己的计时器收起。
void MainWin::updateSnippetSuggestions(const QString& spokenText) {
    if (!m_snippetStore->settings().voiceTrigger || !m_liveOverlay->isVisible()) return;

    QVector<SnippetSuggestBar::Suggestion> suggestions;
    for (const SnippetItem* item : m_snippetStore->matchKeywords(spokenText)) {
        if (m_handledSnippetIds.contains(item->id)) continue;
        const QString title = item->title;
        const QString text = item->text;
        suggestions.push_back({ item->id, title.isEmpty() ? text : title, text });
    }
    if (suggestions.isEmpty()) return;

    // 先定位再设内容：setSuggestions 里会 show，反过来第一帧会出现在上一次的位置
    const QRectF card = m_liveOverlay->cardRect();
    const QPoint topLeft = m_liveOverlay->mapToGlobal(card.topLeft().toPoint());
    m_suggestBar->placeNear(QRect(topLeft, card.size().toSize()), m_liveOverlay->atTop());
    m_suggestBar->setSuggestions(suggestions);
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
        QMetaObject::invokeMethod(m_captionEngine, &StreamingEngine::unloadModel, Qt::QueuedConnection);
        m_captionModel->clear();
    }
}

void MainWin::changeEvent(QEvent* event)
{
    if (QEvent::LanguageChange == event->type()) {
        ui->retranslateUi(this);
        retranslateDynamicItems();
        setDictationIndicator(m_indicatorRecording);  
        refreshLanguageComboTexts();
        alignSettingsCards();
    }
    else if (event->type() == QEvent::PaletteChange || event->type() == QEvent::ApplicationPaletteChange) {
        refreshFormSwitchIcon();   // 图标底板和线条色取自调色板，切主题要重画
    }
    else if (event->type() == QEvent::WindowStateChange) {
        // 没有托盘时隐藏就找不回来了，退回系统的最小化
        if ((windowState() & Qt::WindowMinimized) && QSystemTrayIcon::isSystemTrayAvailable()) {
            hide();
        }
    }
    QMainWindow::changeEvent(event);
}


void MainWin::closeEvent(QCloseEvent* event)
{
    // 关窗口只隐藏，不退出程序；退出走托盘右键菜单，没有托盘的桌面走横条右键菜单。
    // 没有托盘时隐藏就找不回来，改为切到横条
    if (QSystemTrayIcon::isSystemTrayAvailable()) {
        hide();
        setWindowState(Qt::WindowMinimized);
    } else {
        switchToBar();
    }
    event->ignore();
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

    // 尺寸缩不下时也至少保证左上角可见，让标题栏能拖动
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
	// GPU 模式下顺带补齐该模型的 GPU 版（没有 GPU 版 / 已下载则什么都不做）
	if (config.sherpa.useGpu) ensureGpuVariant(repoId);
}

// 首次加载模型的入口：useGpu 为真且 GPU 检测还没结束时先不加载，等检测结果再加载一次。
// 否则会先按 CPU 加载、检测完又按 CUDA 重载，白白多花数秒
void MainWin::requestPreload()
{
    const AppConfig& cfg = ConfigManager::instance().config();
    // 检测结果统一由 GpuRuntime 持有；超时后不再等
    const bool detectionDone = m_detectionTimedOut || m_gpuRuntime->detectionDone();
    if (GpuVariant::shouldWaitForGpuDetection(m_gpuRuntime->requestedGpu(), detectionDone)) {
        if (!m_preloadDeferred) LOG_INFO("Model load deferred until GPU detection finishes");
        m_preloadDeferred = true;
        if (!m_detectTimer) {
            m_detectTimer = new QTimer(this);
            m_detectTimer->setSingleShot(true);
            m_detectTimer->setInterval(kGpuDetectionWaitMs);
            connect(m_detectTimer, &QTimer::timeout, this, [this]() {
                m_detectionTimedOut = true;
                settleGpuDetection("detection timeout");
            });
        }
        if (!m_detectTimer->isActive()) m_detectTimer->start();
        return;
    }
    LOG_INFO(QString("Model load triggered by %1").arg(m_gpuRuntime->requestedGpu() ? "detection already finished" : "useGpu disabled"));
    m_workflow->preloadModel(cfg);
}

void MainWin::settleGpuDetection(const char* reason)
{
    if (m_detectTimer) m_detectTimer->stop();
    if (!m_preloadDeferred) return;
    m_preloadDeferred = false;
    LOG_INFO(QString("Model load triggered by %1").arg(reason));
    m_workflow->preloadModel(ConfigManager::instance().config());
}

void MainWin::onGpuDetectionFinished(bool ready)
{
    if (m_detectionTimedOut) {
        // 已经超时按当前状态加载过了；检测结果才出来且 CUDA 可用时，补一次加载切到 GPU
        m_detectionTimedOut = false;
        if (ready) {
            LOG_INFO("Model load re-triggered by late GPU detection");
            m_workflow->preloadModel(ConfigManager::instance().config());
        }
        return;
    }
    settleGpuDetection("detection finished");
}

void MainWin::onComputeModeChanged(bool toGpu)
{
    AppConfig& cfg = ConfigManager::instance().config();
    const QString repoId = ModelRegistry::FindByDisplayName(cfg.sherpa.languageModel, cfg.sherpa.localModelRepoId);
    LOG_INFO(QString("Compute mode -> %1, selected model=%2").arg(toGpu ? "GPU" : "CPU", repoId));

    // 切回 CPU：自动下载没必要继续，取消并清掉残留
    if (!toGpu && !m_autoGpuRepoId.isEmpty()) {
        LOG_INFO(QString("Cancel auto GPU download: %1").arg(m_autoGpuRepoId));
        m_sherpaInstaller->cancelInstall(m_autoGpuRepoId);
        m_autoGpuRepoId.clear();
    }
    if (repoId.isEmpty()) return;
    if (toGpu) ensureGpuVariant(repoId);
    // 让加载器按新模式选 CPU 版或 GPU 版；版本没变时它会直接复用已加载的识别器
    m_workflow->preloadModel(cfg);
}

void MainWin::ensureGpuVariant(const QString& repoId)
{
    switch (ModelRegistry::GpuStateOf(repoId)) {
    case GpuVariant::GpuState::Unsupported:
        notify(NotifyLevel::Info, tr("当前模型不支持 GPU"), tr("继续使用 CPU 运行"));
        break;
    case GpuVariant::GpuState::Ready:
        break;
    case GpuVariant::GpuState::NotInstalled: {
        const QString gpuRepo = ModelRegistry::GpuVariantOf(repoId);
        if (m_sherpaInstaller->isInstalling(gpuRepo)) break;
        m_autoGpuRepoId = gpuRepo;
        const ModelDescriptor* d = ModelRegistry::Find(gpuRepo);
        const double gb = d ? d->approxSizeMb / 1000.0 : 0.0;
        LOG_INFO(QString("Auto download GPU variant %1 for %2").arg(gpuRepo, repoId));
        notify(NotifyLevel::Info, tr("正在下载 GPU 版"),
               tr("约 %1 GB，下载期间继续使用 CPU，完成后自动切换").arg(gb, 0, 'f', 1));
        m_sherpaInstaller->installModel(gpuRepo);
        break;
    }
    }
}

// 启动时：配置 → 控件。标准控件由 SettingsBinder 回填，其余是登记表里注明的自定义流程
void MainWin::loadConfigToUI() {
    ConfigManager::instance().load();
    AppConfig& cfg = ConfigManager::instance().config();

    m_loadingSettings = true;
    m_settings->loadAll();
    ui->shortcut_edit->setShortCut(cfg.hotkey);
    ui->gpu_backend_widget->setComputeMode(cfg.sherpa.useGpu ? GpuBackendWidget::ComputeMode::CUDA : GpuBackendWidget::ComputeMode::CPU);
    // 平台不可检测/后端未就绪时 runtime 会把模式锁回 CPU，配置要跟着回写
    cfg.sherpa.useGpu = m_gpuRuntime->requestedGpu();
    ui->model_list_widget->setGpuMode(cfg.sherpa.useGpu);
    const int index = ui->language_comb->findData(cfg.sherpa.languageModel);
    if (index != -1) ui->language_comb->setCurrentIndex(index);
    ui->model_list_widget->setCurrentModel(cfg.sherpa.localModelRepoId);
    ui->ai_vocabulary_edit->setText(cfg.polish.vocab);
    rebuildTranslationLanguages();   // 下拉内容依赖已载入的翻译引擎，要在 loadAll 之后按配置重选
    m_workflow->setInjectEnabled(cfg.injectToForeground);
    m_loadingSettings = false;
    m_lastAiEngine = cfg.polish.aiEngineIndex;
    // loadAll 期间设置项的改动不会发通知，启动时的置顶状态要在这里按配置应用一次
    m_controlBar->setAlwaysOnTop(cfg.barAlwaysOnTop);
    m_panel->setAlwaysOnTop(cfg.barAlwaysOnTop);

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
            requestPreload();
        }
    }

    updateAiEngineUi();
    m_ollamaClient->setBaseUrl(cfg.polish.apiUrl);
    rebuildTranslateModelCombo({});
    // 没用 Ollama 的用户不去连本机端口
    if (ui->ai_engine_comb->currentIndex() == 2) m_ollamaClient->checkService();
    requestModelList();
}

// 润色模型按引擎各记一份：换引擎时把当前模型存给旧引擎，换入新引擎自己记的那个（没有则空），
// 并立刻清掉下拉框里旧引擎的模型。否则换到 Ollama 后，没连上 Ollama 时会把 Gemini 的模型名当"已保存的模型"
// 补回列表并选中
void MainWin::swapPolishModelForEngine(int engine) {
    PolishConfig& p = ConfigManager::instance().config().polish;
    if (m_lastAiEngine >= 0 && m_lastAiEngine != engine) p.modelByEngine[m_lastAiEngine] = p.model;
    const int previousEngine = m_lastAiEngine != engine ? m_lastAiEngine : -1;
    m_lastAiEngine = engine;
    p.model = p.modelByEngine.value(engine);
    // 地址和密钥同理：Gemini 的密钥、OpenAI 兼容的地址不能带到 Ollama（地址不对会让 Ollama 一直被判为没运行）
    if (previousEngine >= 0) {
        p.apiUrlByEngine[previousEngine] = p.apiUrl;
        p.apiKeyByEngine[previousEngine] = p.apiKey;
    }
    p.apiUrl = p.apiUrlByEngine.value(engine);
    p.apiKey = p.apiKeyByEngine.value(engine);
    ui->api_url_edit->setText(p.apiUrl);   // setText 不发 editingFinished，不会触发绑定回写
    ui->api_key_edit->setText(p.apiKey);
    if (previousEngine >= 0) p.translateModelByEngine[previousEngine] = p.translateModel;
    p.translateModel = p.translateModelByEngine.value(engine);
    {
        const QSignalBlocker block(ui->ai_model_comb);   // 清空不能触发绑定把 model 写成空
        ui->ai_model_comb->clear();
    }
    ui->ollama_model_btn->setModel(QString());
    // Ollama / 没有模型列表的引擎在服务没响应前也要有内容可选：先用已记住的值和推荐模型占位，拉到列表后再重建
    rebuildPolishModelCombo({}, false);
    rebuildTranslateModelCombo({});
    applySettings(0);
}

void MainWin::requestModelList() {
    const int engine = ui->ai_engine_comb->currentIndex();
    if (!m_textPolishService->capabilities(engine).canFetchModels) return;

    const AppConfig& cfg = ConfigManager::instance().config();
    TextPolishService::RequestParams params;
    params.aiEngine = engine;
    params.apiKey = cfg.polish.apiKey;
    params.customUrl = cfg.polish.apiUrl;
    m_textPolishService->fetchModels(params);
}

// Ollama 下把还没下载的推荐模型也列进下拉框（排在最前），灰字标出；下载由旁边的按钮完成
static QStringList withOllamaCatalog(const QStringList& installed, OllamaPurpose purpose, QStringList* missing) {
    QStringList items = installed;
    for (const QString& name : ollamaCatalogNames(purpose)) {
        const bool has = std::any_of(installed.cbegin(), installed.cend(),
                                     [&name](const QString& m) { return OllamaClient::sameModel(m, name); });
        if (has) continue;
        items.prepend(name);
        if (missing) *missing << name;
    }
    return items;
}

static void markNotDownloaded(QComboBox* combo, int index) {
    if (index < 0) return;
    combo->setItemData(index, combo->palette().color(QPalette::PlaceholderText), Qt::ForegroundRole);
    combo->setItemData(index, QCoreApplication::translate("MainWin", "Not downloaded yet"), Qt::ToolTipRole);
}

void MainWin::rebuildPolishModelCombo(const QStringList& installed, bool listed) {
    const bool ollama = ui->ai_engine_comb->currentIndex() == 2;
    AppConfig& config = ConfigManager::instance().config();
    QStringList missing;
    QStringList items = ollama ? withOllamaCatalog(installed, OllamaPurpose::Polish, &missing) : installed;
    // 服务没开、列表为空时保留已保存的模型，不能因为暂时连不上就把设置冲掉
    if (!listed && !config.polish.model.isEmpty() && !items.contains(config.polish.model)
        && !(ollama && config.polish.model.startsWith(QStringLiteral("gemini-"), Qt::CaseInsensitive))) {
        items << config.polish.model;
        missing << config.polish.model;
    }
    {
        // 重建列表时不能让下拉框的改动信号把已保存的模型名冲掉
        const QSignalBlocker block(ui->ai_model_comb);
        ui->ai_model_comb->clear();
        ui->ai_model_comb->addItems(items);
        if (ollama) {
            for (const QString& m : missing) markNotDownloaded(ui->ai_model_comb, ui->ai_model_comb->findText(m));
        }
        ui->ai_model_comb->setCurrentIndex(std::max(0, ui->ai_model_comb->findText(config.polish.model)));
    }
    ui->ollama_model_btn->setModel(ui->ai_model_comb->currentText());
    // 已保存的模型不在新列表里（如换了引擎）时，改用列表第一项
    if (!ui->ai_model_comb->currentText().isEmpty() && config.polish.model != ui->ai_model_comb->currentText()) {
        config.polish.model = ui->ai_model_comb->currentText();
        applySettings(0);
    }
}

void MainWin::rebuildTranslateModelCombo(const QStringList& models) {
    const bool ollama = ui->ai_engine_comb->currentIndex() == 2;
    const QString saved = ConfigManager::instance().config().polish.translateModel;
    QStringList missing;
    const QStringList items = ollama ? withOllamaCatalog(models, OllamaPurpose::Translate, &missing) : models;
    const QSignalBlocker blocker(ui->translate_model_comb);
    ui->translate_model_comb->clear();
    for (const QString& m : items) {
        // 翻译专用模型标出来，方便挑
        const QString label = TextPolishPrompts::isTranslationModel(m) ? tr("%1 (translation model)").arg(m) : m;
        ui->translate_model_comb->addItem(label, m);
        if (missing.contains(m)) markNotDownloaded(ui->translate_model_comb, ui->translate_model_comb->count() - 1);
    }
    // 列表还没拉到（或服务暂时没开）时保留已保存的值，不能因为暂时连不上就把设置冲掉
    if (!saved.isEmpty() && ui->translate_model_comb->findData(saved) < 0) {
        ui->translate_model_comb->addItem(saved, saved);
        if (ollama) markNotDownloaded(ui->translate_model_comb, ui->translate_model_comb->count() - 1);
    }
    // 不再有"跟随润色模型"：翻译模型始终是一个具体的模型名。没选过时（旧配置里为空 = 原来跟随润色），
    // 沿用润色模型保持行为不变；润色模型不在列表里就挑翻译专用模型，再不行取第一项
    QString chosen = saved;
    if (chosen.isEmpty() && ui->translate_model_comb->count() > 0) {
        const QString polishModel = ConfigManager::instance().config().polish.model;
        int idx = ui->translate_model_comb->findData(polishModel);
        for (int i = 0; idx < 0 && i < ui->translate_model_comb->count(); ++i) {
            if (TextPolishPrompts::isTranslationModel(ui->translate_model_comb->itemData(i).toString())) idx = i;
        }
        chosen = ui->translate_model_comb->itemData(std::max(0, idx)).toString();
    }
    ui->translate_model_comb->setCurrentIndex(std::max(0, ui->translate_model_comb->findData(chosen)));
    ui->ollama_translate_btn->setModel(chosen);
    if (!chosen.isEmpty() && chosen != saved) {
        ConfigManager::instance().config().polish.translateModel = chosen;
        applySettings(0);
    }
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
    if (m_formSwitchBtn) {
        m_formSwitchBtn->setText(tr("Compact mode"));
        m_formSwitchBtn->setToolTip(tr("Switch to the compact toolbar"));
    }
    if (m_pagesInSettings) {
        ui->tabWidget->setTabText(ui->tabWidget->indexOf(m_transcriptWidget), tr("Live Transcript"));
        ui->tabWidget->setTabText(ui->tabWidget->indexOf(m_termWidget), tr("Dictionary"));
        ui->tabWidget->setTabText(ui->tabWidget->indexOf(m_fileWidget), tr("File Transcription"));
    }
    fillCombo(ui->identification_backend_comb, kBackendItems);
    fillCombo(ui->ai_engine_comb, kAiEngineItems);
    fillCombo(ui->ai_style_comb, kStyleItems);
    rebuildTranslationLanguages();

    retranslateAppearanceItems();
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
    bool installing = false;
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
        for (const TranslationHop& hop : route.hops) installing |= m_translationInstaller->isInstalling(hop.from, hop.to);
        if (installing) {
            const QString line = m_translateProgress.line();
            status = line.isEmpty() ? tr("Downloading model...") : tr("Downloading model...") + QStringLiteral(" · ") + line;
        }
        else {
            const double mb = m_translationInstaller->bytesToDownload(route) / 1048576.0;
            status = tr("Model download required: %1 MB").arg(mb, 0, 'f', 0);
            canDownload = true;
        }
    }

    ui->translate_model_status_label->setText(status);
    ui->translate_model_download_btn->setEnabled(transOn && canDownload);
    ui->translate_progress_bar->setVisible(local && installing);

    ui->translate_model_status_label->setVisible(local);
    ui->translate_model_download_btn->setVisible(local);
}

// 润色与翻译相关控件只在这里启用/隐藏：开关关闭时灰显并保留值，"不适用"的块直接隐藏。
// 选本地模型时翻译在本机完成，没有润色，也不需要云端的 Key、地址和模型。
void MainWin::placeModelRow(bool inPolishCard) {
    if (m_modelRowInPolishCard == inPolishCard) return;
    m_modelRowInPolishCard = inPolishCard;
    // 控件换个网格就够了：addWidget 会自动改父控件，信号连接和 SettingsBinder 绑定都不受影响
    // 润色卡片里放在"风格"同一行右侧（第 1 行第 2、3 列），列布局与上面的连接卡片一致
    auto* from = inPolishCard ? static_cast<QGridLayout*>(ui->ai_service_card_layout)
                              : static_cast<QGridLayout*>(ui->polish_card_layout);
    auto* to = inPolishCard ? static_cast<QGridLayout*>(ui->polish_card_layout)
                            : static_cast<QGridLayout*>(ui->ai_service_card_layout);
    from->removeWidget(ui->label_14);
    from->removeWidget(ui->ai_model_row);
    const int row = inPolishCard ? 1 : 0;
    to->addWidget(ui->label_14, row, 2);
    to->addWidget(ui->ai_model_row, row, 3);
}

// Ollama 没有密钥：地址独占第二行，从最左边对齐；其它引擎地址和密钥并排，地址在右半边
void MainWin::placeUrlRow(bool alone) {
    if (m_urlRowAlone == alone) return;
    m_urlRowAlone = alone;
    auto* grid = static_cast<QGridLayout*>(ui->ai_service_card_layout);
    grid->removeWidget(ui->label_16);
    grid->removeWidget(ui->api_url_edit);
    if (alone) {
        grid->addWidget(ui->label_16, 1, 0);
        grid->addWidget(ui->api_url_edit, 1, 1, 1, 4);
    } else {
        grid->addWidget(ui->label_16, 1, 2);
        grid->addWidget(ui->api_url_edit, 1, 3, 1, 2);
    }
}

void MainWin::updateAiEngineUi() {
    const int engine = ui->ai_engine_comb->currentIndex();
    const bool local = engine == kLocalTranslationEngine;
    const bool polishOn = !local && ui->enable_enhancement_checkbox->isChecked();
    const bool transOn = ui->translate_enabled_checkbox->isChecked();
    const auto caps = m_textPolishService->capabilities(engine);

    // 本地翻译后端接入前，这一项只会跳过翻译；灰显避免用户以为能用
    if (auto* model = qobject_cast<QStandardItemModel*>(ui->ai_engine_comb->model())) {
        if (QStandardItem* item = model->item(kLocalTranslationEngine)) {
            const bool available = m_translationService->hasBackend();
            item->setEnabled(available);
            item->setToolTip(available ? QString() : tr("Not available in this version yet"));
        }
    }

    const auto setEnabled = [](const QList<QWidget*>& widgets, bool on) {
        for (QWidget* w : widgets) w->setEnabled(on);
    };
    const auto setVisible = [](const QList<QWidget*>& widgets, bool on) {
        for (QWidget* w : widgets) w->setVisible(on);
    };

    // 引擎下拉始终显示；连接信息只对云端引擎有意义
    setVisible({ ui->label_16, ui->api_url_edit, ui->label_14, ui->ai_model_row, ui->check_connection_btn }, !local);
    // Ollama 不需要密钥：只留地址，留空就用默认的 localhost:11434
    setVisible({ ui->label_13, ui->api_key_edit }, !local && engine != 2);
    ui->api_url_edit->setPlaceholderText(engine == 2 ? tr("Leave empty to use the default (http://localhost:11434)")
                                                     : QString());
    setEnabled({ ui->ai_model_comb, ui->label_14 }, caps.canFetchModels);
    ui->check_connection_btn->setEnabled(caps.canTest);
    // Ollama 下模型下拉框旁多一个下载按钮（所选模型没装时才出现）
    const bool ollama = engine == 2;
    // 润色模型只有一个控件：Gemini / OpenAI 兼容时在连接卡片里，Ollama 时挪进润色卡片，
    // 与翻译模型在翻译卡片里对应（Ollama 的模型是本机下载的，跟着用途走更好理解）
    placeModelRow(ollama);
    placeUrlRow(ollama);
    ui->ollama_model_btn->setActive(ollama);
    ui->ollama_translate_btn->setActive(ollama);
    // 单独指定翻译模型只对能列模型的接口有意义（OpenAI 兼容 / Ollama）
    setVisible({ ui->translate_model_label, ui->translate_model_comb }, engine == 1 || ollama);

    ui->polish_card->setVisible(!local);
    // 自订指令只有风格选"自订Prompt"时才会用到，其余风格下显示出来没有作用
    const bool customStyle = ui->ai_style_comb->currentData().toString() == QStringLiteral("自订Prompt");
    setVisible({ ui->label_17, ui->custom_commands_edit }, customStyle);
    setEnabled({ ui->ai_style_comb, ui->custom_commands_edit, ui->ai_vocabulary_edit,
                 ui->label_18, ui->label_17, ui->label_20 }, polishOn && caps.canPolish);

    // 翻译与润色各自独立：只开翻译时单独发一次翻译请求（见 AiStagePlan）
    setEnabled({ ui->translate_source_comb, ui->translate_source_label,
                 ui->translate_language_comb, ui->label_19, ui->translate_model_label, ui->translate_model_comb }, transOn);
    setVisible({ ui->translate_source_comb, ui->translate_source_label }, local);
    ui->translate_hint_label->clear();
    ui->translate_hint_label->setVisible(false);

    updateTranslationUi();

    // 只列出实际会执行的步骤；润色和翻译都没开时没有额外处理，不显示这一行
    QStringList steps{ tr("Recognition") };
    if (polishOn) steps << tr("Polishing");
    if (transOn) steps << tr("Translation");
    steps << tr("Output");
    ui->pipeline_hint_label->setText(steps.join(QStringLiteral("  →  ")));
    ui->pipeline_hint_label->setVisible(steps.size() > 2);

    // 横条上的两个 AI 开关：和设置页同样的限制，并把原因写进悬停提示
    if (m_controlBar) {
        m_controlBar->toggleButton(ControlBar::Toggle::Enhance)->setEnabled(!local);
        m_controlBar->setToggleHint(ControlBar::Toggle::Enhance,
                                    local ? tr("Not available with the local translation engine") : QString());
        m_controlBar->setToggleHint(ControlBar::Toggle::Translate, QString());
    }
}

// qss 只管写了规则的控件；自绘控件和 qss 里的 palette(...) 跟的是应用调色板。
// Fusion 的默认调色板随系统深浅色，不设的话系统为深色时，白色主题的弹窗会是深底深字
static QPalette themePalette(LiveOverlay::Theme theme) {
    struct Colors { QColor window, base, text, button, midlight, mid, highlight, disabledText; };
    Colors c;
    switch (theme) {
    case LiveOverlay::Theme::Dark:
        c = { QColor(53, 53, 53), QColor(42, 42, 42), QColor(220, 220, 220), QColor(42, 42, 42),
              QColor(80, 80, 80), QColor(90, 90, 90), QColor(0, 122, 204), QColor(120, 120, 120) };
        break;
    case LiveOverlay::Theme::Gray:
        c = { QColor(200, 200, 200), QColor(160, 160, 160), QColor(0, 0, 0), QColor(130, 130, 130),
              QColor(140, 140, 140), QColor(120, 120, 120), QColor(0, 120, 215), QColor(110, 110, 110) };
        break;
    default:
        c = { QColor(245, 245, 245), QColor(255, 255, 255), QColor(30, 30, 30), QColor(240, 240, 240),
              QColor(215, 215, 215), QColor(200, 200, 200), QColor(0, 120, 215), QColor(170, 170, 170) };
        break;
    }

    QPalette pal;
    pal.setColor(QPalette::Window, c.window);
    pal.setColor(QPalette::WindowText, c.text);
    pal.setColor(QPalette::Base, c.base);
    pal.setColor(QPalette::AlternateBase, c.window);
    pal.setColor(QPalette::Text, c.text);
    pal.setColor(QPalette::Button, c.button);
    pal.setColor(QPalette::ButtonText, c.text);
    pal.setColor(QPalette::ToolTipBase, c.base);
    pal.setColor(QPalette::ToolTipText, c.text);
    pal.setColor(QPalette::PlaceholderText, c.disabledText);
    pal.setColor(QPalette::Midlight, c.midlight);
    pal.setColor(QPalette::Mid, c.mid);
    pal.setColor(QPalette::Highlight, c.highlight);
    pal.setColor(QPalette::HighlightedText, Qt::white);
    for (QPalette::ColorRole role : { QPalette::WindowText, QPalette::Text, QPalette::ButtonText })
        pal.setColor(QPalette::Disabled, role, c.disabledText);
    return pal;
}

static void applyAppTheme(LiveOverlay::Theme theme) {
    const QString qss = theme == LiveOverlay::Theme::Dark ? QStringLiteral(":/style/myblackstyle.qss")
                      : theme == LiveOverlay::Theme::Gray ? QStringLiteral(":/style/mygraystyle.qss")
                                                          : QStringLiteral(":/style/mywhitestyle.qss");
    qApp->setStyle(QStyleFactory::create("Fusion"));
    // 先设调色板：setStyleSheet 重新抛光控件时，palette(...) 才会取到新颜色
    qApp->setPalette(themePalette(theme));
    QFile styleFile(qss);
    styleFile.open(QFile::ReadOnly | QFile::Text);
    QTextStream ts(&styleFile);
    qApp->setStyleSheet(ts.readAll());
    styleFile.close();
}

void MainWin::readIni() {
  QString inifile = ConfigManager::configFilePath();
  if (!QFile::exists(inifile)) return;

  QSettings settings(inifile, QSettings::IniFormat);
  style = settings.value("style", "dark").toString();
  setTheme(Appearance::themeFromKey(style), false);

  Language = settings.value("Language", "System").toString();
  setLanguage(Language, false);
}

// ---- 外观：主题与界面语言（外观页） ----

void MainWin::setupAppearanceUi() {
    // 条目的文字在 retranslateAppearanceItems 里按当前语言设置；这里只放占位，保证个数和顺序
    for (int i = 0; i < Appearance::languageKeys().size(); ++i) ui->ui_language_comb->addItem(QString());
    for (int i = 0; i < Appearance::themeKeys().size(); ++i) ui->theme_comb->addItem(QString());

    connect(ui->ui_language_comb, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        if (index < 0) return;
        setLanguage(Appearance::languageKeys().at(index), true);
    });
    connect(ui->theme_comb, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        if (index < 0) return;
        setTheme(Appearance::themeAt(index), true);
    });
}

void MainWin::retranslateAppearanceItems() {
    const QSignalBlocker blockLang(ui->ui_language_comb);
    const QSignalBlocker blockTheme(ui->theme_comb);
    ui->ui_language_comb->setItemText(0, tr("Follow system"));
    ui->ui_language_comb->setItemText(1, QStringLiteral("中文"));      // 语言名用它自己的文字，换了界面语言也认得出
    ui->ui_language_comb->setItemText(2, QStringLiteral("English"));
    ui->theme_comb->setItemText(0, tr("Light"));
    ui->theme_comb->setItemText(1, tr("Gray"));
    ui->theme_comb->setItemText(2, tr("Dark"));
}

void MainWin::syncAppearanceCombos() {
    const QSignalBlocker blockLang(ui->ui_language_comb);
    const QSignalBlocker blockTheme(ui->theme_comb);
    ui->ui_language_comb->setCurrentIndex(Appearance::languageIndex(Language));
    ui->theme_comb->setCurrentIndex(Appearance::themeIndex(Appearance::themeFromKey(style)));
}

void MainWin::setTheme(LiveOverlay::Theme theme, bool persist) {
    applyAppTheme(theme);
    m_liveOverlay->setTheme(theme);
    style = Appearance::themeKey(theme);
    if (persist) {
        // 和原来菜单里的行为一致：深色、灰色会把全局字体设成 Arial 9，浅色不动
        if (theme != LiveOverlay::Theme::Light) qApp->setFont(QFont("Arial", 9));
        QSettings setting(ConfigManager::configFilePath(), QSettings::IniFormat);
        setting.setValue("style", style);
    }
    syncAppearanceCombos();
}

void MainWin::setLanguage(const QString& key, bool persist) {
    Language = Appearance::languageKeys().contains(key) ? key : QStringLiteral("System");
    if (persist) {
        QSettings setting(ConfigManager::configFilePath(), QSettings::IniFormat);
        setting.setValue("Language", Language);
    }

    // 先拆再装：同一个翻译器对象重复安装会叠加；拆和装各发一次语言变化事件，界面据此重新翻译
    qApp->removeTranslator(&mTranslator);
    if (mTranslator.load(Appearance::translationResource(Language, QLocale::system().name()))) {
        qApp->installTranslator(&mTranslator);
    }
    syncAppearanceCombos();
}


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
    if (m_fileWidget->isBusy()) {
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
    m_transcriptWidget->setRecording(state != WorkflowState::Idle);
    m_controlBar->setWorkflowState(state);

    switch (state) {
    case WorkflowState::Loading:
        // 新一轮录音：旧按钮对应的是上一句话，留着容易误点
        m_suggestBar->dismiss();
        m_handledSnippetIds.clear();
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
            if (m_workflow->state() != WorkflowState::Idle) return;
            // 还要陪指令条留一会儿，转录已结束，不能继续闪"转录中"的圆点
            if (m_suggestBar->isVisible()) m_liveOverlay->setPaused();
            m_liveOverlay->hideOverlay();
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

void MainWin::onNewLogEntry(const QString& entry)
{
    if (ui && ui->config_log_edit) {
        QTextCursor cursor = ui->config_log_edit->textCursor();
        cursor.movePosition(QTextCursor::End);

        // 只按 "[时间] LEVEL | 正文" 里的级别字段着色；正文里的 error=0 之类文字不能让 DEBUG 行变红
        const int levelStart = entry.indexOf("] ");
        const int levelEnd = levelStart < 0 ? -1 : entry.indexOf(" | ", levelStart);
        const QString level = levelEnd < 0 ? QString() : entry.mid(levelStart + 2, levelEnd - levelStart - 2);

        QTextCharFormat format;
        if (level == QLatin1String("ERROR")) {
            format.setForeground(QColor("#FF3333"));
            format.setFontWeight(QFont::Bold);
        }
        else if (level == QLatin1String("WARN")) {
            format.setForeground(QColor("#FFA500"));
            format.setFontWeight(QFont::Bold);
        }
        else if (level == QLatin1String("DEBUG")) {
            format.setForeground(QColor("#007799"));
            format.setFontWeight(QFont::Normal);
        }
        else if (level == QLatin1String("INFO")) {
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
    if (ui->about_page->isUserDriven()) return;

    QMessageBox msgBox(this);
    msgBox.setWindowTitle(tr("Update Available"));
    msgBox.setText(tr("A new version (%1) is available!").arg(version));
    msgBox.setInformativeText(tr("Would you like to download and install it now?"));

    msgBox.setDetailedText(notes);
    msgBox.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
    msgBox.setDefaultButton(QMessageBox::Ok);
    msgBox.setIcon(QMessageBox::Information);

    // "不再提醒"复选项：勾选后持久化，后续启动不再弹更新提示
    auto* neverAgain = new AnimatedCheckBox;
    neverAgain->setLabelText(tr("Do not remind me again"));
    msgBox.setCheckBox(neverAgain);

    int ret = msgBox.exec();

    if (neverAgain->isChecked()) {
        ConfigManager::instance().config().skipUpdateReminder = true;
        ConfigManager::instance().save();
        LOG_INFO("User opted out of update reminders");
    }

    if (ret == QMessageBox::Ok) {
        m_updateManager->downloadUpdate(UpdateManager::defaultDownloadPath());
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
        }
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
    // 托盘只管显示与隐藏和退出，其余功能都在横条和设置窗口里
    trayMenu->clear();
    const bool shown = isVisible() || m_controlBar->isVisible();
    LOG_DEBUG(QString("Tray | menu about to show, settings=%1 bar=%2").arg(isVisible()).arg(m_controlBar->isVisible()));
    trayMenu->addAction(shown ? tr("隐藏") : tr("显示"), this, &MainWin::toggleCurrentForm);
    if (m_liveOverlay->userHidden()) {
        trayMenu->addAction(tr("Show dictation window"), this, [this]() {
            m_liveOverlay->restoreFromUserHide();
            // 正在录音 / 转录时马上显示出来，空闲时等下一次说话自己弹出
            if (m_workflow->state() != WorkflowState::Idle) m_liveOverlay->showOnScreen();
        });
    }
    trayMenu->addAction(tr("退出"), this, &MainWin::onQuitApplication);
}

void MainWin::onTrayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
    LOG_DEBUG(QString("Tray | activated reason=%1").arg(int(reason)));
    if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
        toggleCurrentForm();
    }
}

void MainWin::toggleCurrentForm()
{
    LOG_DEBUG(QString("Tray | toggleCurrentForm settings=%1 bar=%2").arg(isVisible()).arg(m_controlBar->isVisible()));
    if (isVisible()) onHideWindow();
    else if (m_controlBar->isVisible()) hideControlBar();
    else showCurrentForm();
}

void MainWin::showCurrentForm()
{
    if (ConfigManager::instance().config().barMode) {
        movePagesTo(false);
        showControlBar();
    } else {
        movePagesTo(true);
        onShowWindow();
    }
}

void MainWin::movePagesTo(bool settingsWindow)
{
    // 实时转录、词典、文件转录页只有一个实例，在面板和设置窗口的标签页之间借用；状态（稿子、文件转录进度等）留在实例里
    if (m_pagesInSettings == settingsWindow) return;
    if (settingsWindow) {
        m_panel->showPage(PanelState::Page::None);
        m_panel->releaseSharedPages();
        const int at = ui->tabWidget->indexOf(ui->appearance);
        ui->tabWidget->insertTab(at, m_transcriptWidget, tr("Live Transcript"));
        ui->tabWidget->insertTab(at + 1, m_termWidget, tr("Dictionary"));
        ui->tabWidget->insertTab(at + 2, m_fileWidget, tr("File Transcription"));
    } else {
        ui->tabWidget->removeTab(ui->tabWidget->indexOf(m_transcriptWidget));
        ui->tabWidget->removeTab(ui->tabWidget->indexOf(m_termWidget));
        ui->tabWidget->removeTab(ui->tabWidget->indexOf(m_fileWidget));
        m_panel->takeBackSharedPages(m_transcriptWidget, m_termWidget, m_fileWidget);
    }
    m_pagesInSettings = settingsWindow;
}

void MainWin::refreshFormSwitchIcon()
{
    if (!m_formSwitchBtn) return;
    // 和横条上的图标同一套底板；悬停时 Qt 取 Active 模式的图
    const QPalette pal = palette();
    const QColor glyph = ToolIcon::stateColor(pal, ToolIcon::State::Normal);
    constexpr int kSize = 22;
    QIcon icon;
    for (const bool hover : { false, true }) {
        QPixmap pm = QPixmap::fromImage(ToolIcon::renderTile(QStringLiteral(":/tool_compact"), kSize * 2, pal, QColor(), glyph, hover));
        pm.setDevicePixelRatio(2.0);
        icon.addPixmap(pm, hover ? QIcon::Active : QIcon::Normal);
    }
    m_formSwitchBtn->setIcon(icon);
}

void MainWin::switchToBar()
{
    const QPoint at = pos();
    ConfigManager::instance().config().barMode = true;
    ConfigManager::instance().save();
    movePagesTo(false);
    hide();
    m_controlBar->showAt(at);
    m_controlBar->raise();
    ForegroundWindow::bringToFront(m_controlBar->winId());
}

void MainWin::switchToSettings()
{
    const QPoint at = m_controlBar->pos();
    ConfigManager::instance().config().barMode = false;
    ConfigManager::instance().save();
    hideControlBar();
    movePagesTo(true);
    move(FloatingWindow::place(at, size(), FloatingWindow::availableScreens(), FloatingWindow::primaryScreenArea()));
    onShowWindow();
}

void MainWin::showControlBar()
{
    m_controlBar->showAtSavedPosition();
    m_controlBar->raise();
    ForegroundWindow::bringToFront(m_controlBar->winId());
}

void MainWin::hideControlBar()
{
    // 面板紧贴横条，横条藏起来时面板不能留在原地
    m_panel->showPage(PanelState::Page::None);
    m_controlBar->hide();
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
    LOG_DEBUG("Quit requested");
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
