#include "MainWin.h"
#include "ui_MainWin.h"

#include <QActionGroup>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QStyleFactory>
#include <QTextStream>
#include <QThread>
#include <QTimer>
#include <QListWidget>
#include <QDesktopServices>

#include "AppConfig.h"
#include "WorkflowManager.h"
#include "ConfigManager.h"
#include "UpdateManager.h"
#include "TermsLibraryManager.h"
#include "sherpa/SherpaManager.h"
#include "utils/Logger.h"

#include "widgets/SphereOverlay.h"
#include "widgets/AboutDialog.h"
#include "widgets/NavListWidget.h"
#include "widgets/inforbar/inforbarmanager.h"
#include "widgets/inforbar/inforposmanager.h"

MainWin::MainWin(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWin) {
  ui->setupUi(this);
  ui->ai_vocabulary_edit->setReadOnly(true);
  ui->identification_log_edit->setReadOnly(true);
  ui->config_log_edit->setReadOnly(true);

  ui->grop_key_label->setVisible(false);
  ui->grop_key_edit->setVisible(false);
  ui->gladia_key_edit->setVisible(false);
  ui->gladia_key_label->setVisible(false);
  ui->gemini_key_edit->setVisible(false);
  ui->gemini_key_label->setVisible(false);

  initSystemTray();
  initialize();
  connection();
  onLoadConfig();
}
    
void MainWin::initialize() {
    ui->recording_equ_comb->clear();
    auto items = AudioRecorderService::availableMicrophones();
    ui->recording_equ_comb->addItems(items);

    ui->source_equipment_comb->clear();
    auto speakers = AudioRecorderService::availableSpeakers();
    ui->source_equipment_comb->addItems(speakers);

    auto languages = ModelRegistry::GetLanguages();
    ui->language_comb->addItems(languages);

    auto models = ModelRegistry::GetModelsByLanguage(ui->language_comb->currentText());
    ui->local_model_comb->addItems(models);

    QStringList backend_items;
    backend_items << "本地识别(Sherpa)";
    backend_items << "Gemini 语音直输";
    backend_items << "Groq 语音直输";
    backend_items << "Gladia 语音直输";
    ui->identification_backend_comb->addItems(backend_items);

    QStringList ai_engine_items;
    ai_engine_items << "Google Gemini API";
    ai_engine_items << "本地/自订 (OpenAI 兼容)";
    ai_engine_items << "Ollama (本地)";
    ui->ai_engine_comb->addItems(ai_engine_items);

    QStringList gemini_style_items;
    gemini_style_items << "智慧预设";
    gemini_style_items << "商务正式";
    gemini_style_items << "日常口語";
    gemini_style_items << "简洁扼要";
    gemini_style_items << "自订Prompt";
    ui->ai_style_comb->addItems(gemini_style_items);

    QStringList translate_language_comb;
    translate_language_comb << "不翻译";
    translate_language_comb << "英文";
    translate_language_comb << "日文";
    translate_language_comb << "韩文";
    translate_language_comb << "繁体中文";  
    translate_language_comb << "简体中文";
    ui->translate_language_comb->addItems(translate_language_comb);

    ui->nav_list_widget->addNavItem("identification", "", "声音识别");
    ui->nav_list_widget->addNavItem("config", "", "日志配置");
    ui->nav_list_widget->addNavItem("download", "", "下载列表");

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
    m_recorderService = new AudioRecorderService(config, this);
    m_sherpaManager = new SherpaManager(this);
    m_sherpaInstaller = new SherpaInstaller(m_networkManager, this);
    m_cudaInstaller = new CudaInstaller(m_networkManager, m_sherpaInstaller, this);

    m_transcriptionService = new TranscriptionService(m_networkManager, m_sherpaManager, m_textPolishService, config, this);
    m_termsManager = new TermsLibraryManager(this);
    m_updateManager = new UpdateManager(m_networkManager, this);

    m_workflow = new WorkflowManager(config, this);
    m_workflow->initialize(m_recorderService, m_transcriptionService, m_sherpaManager);

    // 初始化动画
    m_drawerAnimation = new QPropertyAnimation(ui->drawer_stack, "maximumHeight", this);
    m_drawerAnimation->setDuration(300);
    m_drawerAnimation->setEasingCurve(QEasingCurve::InOutQuart);

    // 声音控件
    m_sphereOverlay = new SphereOverlay();
    //m_sphereOverlay->showAtBottomCenter();
    //m_sphereOverlay->setListening();

    auto* periodicTimer = new QTimer(this);
    periodicTimer->setInterval(4 * 60 * 60 * 1000);
    connect(periodicTimer, &QTimer::timeout, this, [this]() { m_updateManager->checkForUpdates(); });
    periodicTimer->start();

    ui->download_list_widget->setSherpaInstaller(m_sherpaInstaller);
    ui->download_list_widget->setCudaInstaller(m_cudaInstaller);
    ui->gpu_backend_widget->setBackendInstaller(m_cudaInstaller);
    ui->terms_widget->setTermsManager(m_termsManager);
}   

void MainWin::connection(){
    auto& configManager = ConfigManager::instance();

	connect(ui->actionAbout, &QAction::triggered, this, [this]() {
        AboutDialog dialog(this);
        dialog.exec();
	});

    // Basic Setting
	connect(ui->shortcut_edit, &ShortcutEdit::hotkeyActivated, this, &MainWin::onHotkeyPressed);

    connect(ui->identification_backend_comb, &QComboBox::currentIndexChanged, this, [this](int index) {
        bool gropVisible = false;
        bool geminiVisible = false;
        bool gladiaVisible = false;
        
        if (index == 0) {
            gropVisible = false;
            geminiVisible = false;
            gladiaVisible = false;
        }
        else if (index == 1) {
            gropVisible = false;
            geminiVisible = true;
            gladiaVisible = false;
        }else if (index == 2) {
            gropVisible = true;
            geminiVisible = false;
            gladiaVisible = false;
        }
        else if (index == 3) {
            gropVisible = false;
            geminiVisible = false;
            gladiaVisible = true;
        }

        ui->grop_key_label->setVisible(gropVisible);
        ui->grop_key_edit->setVisible(gropVisible);
        ui->gladia_key_edit->setVisible(gladiaVisible);
        ui->gladia_key_label->setVisible(gladiaVisible);
        ui->gemini_key_edit->setVisible(geminiVisible);
        ui->gemini_key_label->setVisible(geminiVisible);
      });

    // 输出设备测试播放（验证虚拟声卡 loopback）
    connect(ui->test_output_btn, &QPushButton::clicked, this, [this]() {
        QString name = ui->source_equipment_comb->currentText();
        m_recorderService->setOutputDevice(name);
        m_recorderService->playTestTone();
        });

    // Local Recognition
    connect(ui->uninstall_sherpa_btn, &QPushButton::clicked, this, [this]() {
        m_sherpaInstaller->uninstallAll();
    });

    connect(ui->uninstall_model_btn, &QPushButton::clicked, this, [this]() {
        AppConfig uiConfig = extractConfigFromUI();
        ConfigManager::instance().updateConfig(uiConfig);
        ConfigManager::instance().save();

        const AppConfig& config = ConfigManager::instance().config();
        QString repoId = ModelRegistry::FindByDisplayName(config.sherpa.languageModel,
                                                        config.sherpa.localModelRepoId);
        if (repoId.isEmpty()) {
            LOG_ERROR("没有找到对应的模型!");
            return;
        }
        LOG_DEBUG(QString("Download %1...").arg(repoId));
        m_sherpaInstaller->uninstallModel(repoId);
    });

	connect(ui->language_comb, &QComboBox::currentIndexChanged, this, [this](int index) {
		QString language = ui->language_comb->itemText(index);
		auto models = ModelRegistry::GetModelsByLanguage(language);
		ui->local_model_comb->clear();
		ui->local_model_comb->addItems(models);
	});

    connect(ui->download_model_btn, &QPushButton::clicked, this, [this]() {
        AppConfig uiConfig = extractConfigFromUI();
        ConfigManager::instance().updateConfig(uiConfig);
        ConfigManager::instance().save();

        const AppConfig& config = ConfigManager::instance().config();
        QString repoId = ModelRegistry::FindByDisplayName(config.sherpa.languageModel, 
                                                          config.sherpa.localModelRepoId);
        if (repoId.isEmpty()) {
            LOG_ERROR("没有找到对应的模型!");
            return;
        }

        LOG_DEBUG(QString("Download %1...").arg(repoId));
        m_sherpaInstaller->installModel(repoId);
    });

    connect(m_sherpaInstaller, &SherpaInstaller::installGroupStarted, this, [=]() {
        ui->download_model_btn->setEnabled(false);
        ui->download_model_btn->setText(tr("Download..."));
    });
    connect(m_sherpaInstaller, &SherpaInstaller::installGroupFinished, this, [=]() {
        ui->download_model_btn->setEnabled(true);
        ui->download_model_btn->setText(tr("Download and configure"));
    });

    connect(ui->open_path_btn, &QPushButton::clicked, this, [this]() {
        QString modelPath = ModelConfigFactory::getSherpaModel();
        QString nativePath = QDir::toNativeSeparators(modelPath);
        QUrl url = QUrl::fromLocalFile(nativePath);
        bool success = QDesktopServices::openUrl(url);
        if (!success) {
            LOG_ERROR(QString("无法打开路径！原始路径: %1 转换后URL: %2").arg(modelPath).arg(url.toString()));
        }
    });

    connect(ui->gpu_backend_widget, &GpuBackendWidget::detectFinished, this, [this](bool result) {
        ConfigManager::instance().config().sherpa.useGpu = result;
        ConfigManager::instance().save();
    });

    // gemini
    connect(ui->check_connection_btn, &QPushButton::clicked, this, [this]() {
        const AppConfig& config = ConfigManager::instance().config();
        TextPolishService::RequestParams params;
        params.aiEngine = config.polish.aiEngineIndex;
        params.apiKey = config.polish.apiKey;
        params.customUrl = config.polish.apiUrl;
        params.model = config.polish.model;
        params.style = config.polish.aiStyle;
        params.customPrompt = config.polish.prompt;
        params.aiVocab = config.polish.vocab;
        params.targetLang = config.polish.targetLang;
        m_textPolishService->testConnection(params);
    });

    connect(ui->nav_list_widget, &QListWidget::currentItemChanged, this, [this]() {
        onToggleDrawer(ui->nav_list_widget->currentRow());
    });

    connect(ui->ai_engine_comb, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        const AppConfig& config = ConfigManager::instance().config();
        TextPolishService::RequestParams params;
        params.aiEngine = index;
        params.apiKey = config.polish.apiKey;
        params.customUrl = config.polish.apiUrl;
        m_textPolishService->fetchModels(params);
    });

    connect(m_textPolishService, &TextPolishService::modelsFetched, this, [this](bool success, const QStringList& models, const QString&) {
        if (!success || models.isEmpty()) {
            LOG_ERROR("获取模型列表失败");
            ui->ai_model_comb->clear();
            return;
        }

        LOG_INFO("获取模型列表成功");
        ui->ai_model_comb->clear();
        ui->ai_model_comb->addItems(models);

        const AppConfig& config = ConfigManager::instance().config();
        int index = ui->ai_model_comb->findText(config.polish.model);
        if (index < 0) index = 0;
        ui->ai_model_comb->setCurrentIndex(index);
    });

    // 更新检查
    connect(ui->actionUpdate, &QAction::triggered, this, [this]() {  m_updateManager->checkForUpdates(); });
    connect(m_updateManager, &UpdateManager::updateAvailable, this, &MainWin::onUpdateFound);
    connect(m_updateManager, &UpdateManager::downloadFinished, this, &MainWin::onUpdateDownloaded);
    connect(m_updateManager, &UpdateManager::updateError, this, [this](const QString& message) { LOG_ERROR(message); });

    // 日志输出
    connect(&Logger::instance(), &Logger::newLogEntry, this, &MainWin::onNewLogEntry, Qt::QueuedConnection);

    // 主要工作流
    connect(m_workflow, &WorkflowManager::transcriptionResultReady, this, [this](const QString& text) {
        QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
        QString result = QString("[%1] %2").arg(timestamp, text);
        ui->identification_log_edit->append(result);
    });
    connect(m_workflow, &WorkflowManager::stateChanged, this, &MainWin::onStateChanged);

    // 录音服务
    connect(m_recorderService, &AudioRecorderService::levelUpdated, m_sphereOverlay, &SphereOverlay::setLevel);
    connect(m_recorderService, &AudioRecorderService::spectrumUpdated, m_sphereOverlay, &SphereOverlay::setSpectrumLevels);

    connect(m_recorderService, &AudioRecorderService::voiceStarted, this, [this]() {
        m_sphereOverlay->hideTimerStop();        
        if (m_workflow->currentState() == WorkflowState::Recording) {
            m_sphereOverlay->setListening();
            m_sphereOverlay->showAtBottomCenter();
        }
    });

    connect(m_recorderService, &AudioRecorderService::voiceStopped, this, [this]() {
        m_sphereOverlay->hideTimerStart();   
    });

	// Sherpa 模型安装器
    connect(m_sherpaInstaller, &SherpaInstaller::loadModel,
        this, [this](const QString& repoId, bool success, const QString& msg) {
            if (success && !repoId.isEmpty()) {
                const AppConfig& config = ConfigManager::instance().config();
                m_sherpaManager->loadModelAsync(config, false);
            }
        });

    connect(m_sherpaManager, &SherpaManager::modelLoadFinished, this, [this](bool ok) {
        if (ok) LOG_INFO("模型加载完成");
        else LOG_ERROR("模型加载失败");
        });

    connect(m_sphereOverlay, &SphereOverlay::sphereClicked, this, [this]() {
        if (m_recorderService->isPaused()) {
            m_recorderService->resume();
            m_sphereOverlay->setListening();
        }else {
            m_recorderService->pause();
            m_sphereOverlay->setPaused();
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

    connect(ui->setting_save_btn, &QPushButton::clicked, this, &MainWin::onSaveConfig);
    connect(ui->setting_cancel_btn, &QPushButton::clicked, this, &MainWin::onLoadConfig);
}

MainWin::~MainWin() {
  onSaveConfig();
  delete ui;
}

void MainWin::changeEvent(QEvent* event)
{
    if (QEvent::LanguageChange == event->type()) {
        ui->retranslateUi(this);
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
}

AppConfig MainWin::extractConfigFromUI() {
    AppConfig uiConfig;
    // 基础/全局设置 (Global/Misc)
    uiConfig.backend = static_cast<AsrBackendKind>(ui->identification_backend_comb->currentIndex());
    uiConfig.continuousMode = ui->autiomatic_monitoring_checkbox->isChecked();
    uiConfig.hotkey = ui->shortcut_edit->getShortCut();
    uiConfig.sherpa.threads = ui->cpu_thread_number_spin->value();

    // 音频设置 (Audio Group)
    uiConfig.audio.deviceId = ui->recording_equ_comb->currentIndex();
    uiConfig.audio.deviceName = ui->recording_equ_comb->currentText();
    uiConfig.audio.outputDeviceName = ui->source_equipment_comb->currentText();
    uiConfig.audio.voiceThreshold = ui->volume_threshold_spin->value();
    uiConfig.audio.silenceTimeoutMs = ui->mute_duration_spin->value();
    uiConfig.audio.minRecordMs = ui->shortest_recording_spin->value();
    uiConfig.audio.maxRecordMs = ui->longest_recording_spin->value();
    uiConfig.audio.sampleRate = 16000; 
    uiConfig.audio.channels = 1;
    uiConfig.audio.bitsPerSample = 16;

    // Sherpa 设置 (Sherpa Group)
    uiConfig.sherpa.useGpu = ui->gpu_backend_widget->currentComputeMode() == GpuBackendWidget::ComputeMode::CUDA;
    uiConfig.sherpa.threads = 4; 
    uiConfig.sherpa.languageModel = ui->language_comb->currentText();
	uiConfig.sherpa.localModelRepoId = ui->local_model_comb->currentText();

    // /AI 设置 (AI Group)
    uiConfig.polish.aiEngineIndex = ui->ai_engine_comb->currentIndex();
    uiConfig.polish.enableAiEnhancement = ui->enable_enhancement_checkbox->isChecked();
    uiConfig.polish.model = ui->ai_model_comb->currentText();
    uiConfig.polish.aiStyle = ui->ai_style_comb->currentText();
    uiConfig.polish.apiUrl = ui->api_url_edit->text();
    uiConfig.polish.apiKey = ui->api_key_edit->text();
    uiConfig.polish.targetLang = ui->translate_language_comb->currentText();
    uiConfig.polish.vocab = ui->ai_vocabulary_edit->text();
    uiConfig.polish.prompt = ui->custom_commands_edit->text();

    // 快捷键 (Hotkey)
    uiConfig.Language = Language;
    uiConfig.style = style;

	// 词典设置 (Terms Group)
    auto rules = m_termsManager->buildAllRules();
    uiConfig.replaceRules = rules.replaceRules;
    uiConfig.polish.vocab = rules.aiVocab;

    QString hotwords;
    for (const QString& w : rules.hotwords) hotwords += w + '\n';
    uiConfig.sherpa.hotwords = hotwords;
    return uiConfig;
}

void MainWin::loadConfigToUI() {
    ConfigManager::instance().load();
    const AppConfig& cfg = ConfigManager::instance().config();

    // 基础/全局
    ui->autiomatic_monitoring_checkbox->setChecked(cfg.continuousMode);
    ui->shortcut_edit->setShortCut(cfg.hotkey);
	ui->identification_backend_comb->setCurrentIndex(static_cast<int>(cfg.backend));
    ui->cpu_thread_number_spin->setValue(cfg.sherpa.threads);
    
    ui->recording_equ_comb->setCurrentIndex(cfg.audio.deviceId);
    {
        int idx = ui->recording_equ_comb->findText(cfg.audio.deviceName);
        if (idx != -1) ui->recording_equ_comb->setCurrentIndex(idx);
    }
    ui->source_equipment_comb->setCurrentIndex(0);
    {
        int oidx = ui->source_equipment_comb->findText(cfg.audio.outputDeviceName);
        if (oidx != -1) ui->source_equipment_comb->setCurrentIndex(oidx);
    }
    ui->volume_threshold_spin->setValue(cfg.audio.voiceThreshold);
    ui->mute_duration_spin->setValue(cfg.audio.silenceTimeoutMs);
    ui->shortest_recording_spin->setValue(cfg.audio.minRecordMs);
    ui->longest_recording_spin->setValue(cfg.audio.maxRecordMs);

    // Sherpa
    ui->gpu_backend_widget->setComputeMode(cfg.sherpa.useGpu ? GpuBackendWidget::ComputeMode::CUDA : GpuBackendWidget::ComputeMode::CPU);
    int index = ui->language_comb->findText(cfg.sherpa.languageModel);
    if (index != -1) ui->language_comb->setCurrentIndex(index);
    int localIndex = ui->local_model_comb->findText(cfg.sherpa.localModelRepoId);
    if (localIndex != -1) ui->local_model_comb->setCurrentIndex(localIndex);

    if (cfg.backend == AsrBackendKind::Sherpa) {
		m_sherpaManager->loadModelAsync(cfg, false);
    }

    // AI Enhancement
    ui->ai_engine_comb->setCurrentIndex(cfg.polish.aiEngineIndex);

    ui->enable_enhancement_checkbox->setChecked(cfg.polish.enableAiEnhancement);
    ui->api_url_edit->setText(cfg.polish.apiUrl);
    ui->api_key_edit->setText(cfg.polish.apiKey);
    ui->ai_vocabulary_edit->setText(cfg.polish.vocab);
    ui->custom_commands_edit->setText(cfg.polish.prompt);
    ui->translate_language_comb->setCurrentText(cfg.polish.targetLang);
    ui->ai_style_comb->setCurrentText(cfg.polish.aiStyle);

    TextPolishService::RequestParams params;
    params.aiEngine = cfg.polish.aiEngineIndex;
    params.apiKey = cfg.polish.apiKey;
    params.customUrl = cfg.polish.apiUrl;

    m_textPolishService->fetchModels(params);

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
  } else if (style == "gray") {
    QString style = "Fusion";
    qApp->setStyle(QStyleFactory::create(style));
    QFile styleFile(":/style/mygraystyle.qss");
    styleFile.open(QFile::ReadOnly | QFile::Text);
    QTextStream ts(&styleFile);
    qApp->setStyleSheet(ts.readAll());
    styleFile.close();

    ui->action_gray->setChecked(true);
  } else {
    QString style = "Fusion";
    qApp->setStyle(QStyleFactory::create(style));
    QFile styleFile(":/style/mywhitestyle.qss");
    styleFile.open(QFile::ReadOnly | QFile::Text);
    QTextStream ts(&styleFile);
    qApp->setStyleSheet(ts.readAll());
    styleFile.close();

    ui->action_light->setChecked(true);
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
    const auto& config = ConfigManager::instance().config();

    QString repoId = ModelRegistry::FindByDisplayName(config.sherpa.languageModel,
                                                      config.sherpa.localModelRepoId);
    if (m_sherpaInstaller->isInstalling(repoId)){
        LOG_ERROR("当前模型正在安装中");
        return;
    }

    if (m_workflow->currentState() == WorkflowState::Idle) {
        m_workflow->startRecording();
        ui->shortcut_edit->setListening(true);   
    }
    else {
        m_workflow->stopRecording();
        ui->shortcut_edit->setListening(false);  
    }
}

void MainWin::onStateChanged(WorkflowState state)
{
    switch (state) {
    case WorkflowState::Loading:
        m_sphereOverlay->setLoading();
        m_sphereOverlay->showAtBottomCenter();
        break;
    case WorkflowState::Recording:
        m_sphereOverlay->setListening();
        m_sphereOverlay->showAtBottomCenter();
        break;
    case WorkflowState::Transcribing:
        m_sphereOverlay->hideTimerStop();
        m_sphereOverlay->setTranscribe();
        m_sphereOverlay->showAtBottomCenter();
        break;
    case WorkflowState::Processing:
        m_sphereOverlay->hideTimerStart();
        break;
    case WorkflowState::Idle:
        QTimer::singleShot(200, [this]() {
            m_sphereOverlay->hideOverlay();
        });
        break;
    case WorkflowState::Error:
        break;
    default:
        break;
    }
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
            InforBar::error("", entry, InforBarPosition::I_BOTTOM_RIGHT, this);
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
            InforBar::success("", entry, InforBarPosition::I_BOTTOM_RIGHT, this);
        }
        else {
            format.setForeground(QColor("#333333"));
            format.setFontWeight(QFont::Normal);
        }

        cursor.setCharFormat(format);
        cursor.insertText(entry + "\n");
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

    int ret = msgBox.exec();

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
            QMessageBox::critical(this, tr("Error"), err);
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

    m_recorderService->updateConfig();
}

void MainWin::onSaveConfig()
{
    LOG_DEBUG("Save Configuration");
    AppConfig uiConfig = extractConfigFromUI();
    ConfigManager::instance().updateConfig(uiConfig);

    if (ConfigManager::instance().save()) {
        LOG_INFO("配置保存成功");
		LOG_DEBUG("Save Configuration Success");

        // 重新加载模型;
        if (uiConfig.backend == AsrBackendKind::Sherpa) {
            m_sherpaManager->reloadModel(uiConfig);
        }
        m_recorderService->updateConfig();
        // 应用输出设备（虚拟声卡 loopback 场景）
        m_recorderService->setOutputDevice(uiConfig.audio.outputDeviceName);
    }
    else {
        LOG_ERROR("Save Configuration Failed");
    }
    
}

void MainWin::initSystemTray()
{
    // 创建托盘菜单
    trayMenu = new QMenu(this);

    // 创建菜单动作
    showAction = trayMenu->addAction("显示(&S)");
    connect(showAction, &QAction::triggered, this, &MainWin::onShowWindow);

    hideAction = trayMenu->addAction("隐藏(&H)");
    connect(hideAction, &QAction::triggered, this, &MainWin::onHideWindow);

    separatorAction = trayMenu->addSeparator();

    quitAction = trayMenu->addAction("退出(&Q)");
    connect(quitAction, &QAction::triggered, this, &MainWin::onQuitApplication);

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

