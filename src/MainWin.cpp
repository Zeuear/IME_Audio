#include "MainWin.h"
#include "ui_mainwin.h"

#include <QActionGroup>
#include <QClipboard>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QProcess>
#include <QRegularExpression>
#include <QStyleFactory>
#include <QTextStream>
#include <QThread>
#include <QTimer>
#include <QDesktopServices>

#include "ConfigManager.h"
#include "UpdateManager.h"
#include "TermsLibraryManager.h"
#include "sherpa/SherpaManager.h"
#include "utils/Logger.h"
#include "widgets/SphereOverlay.h"
#include "qhotkey.h"

MainWin::MainWin(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWin) {
  ui->setupUi(this);

  m_networkManager = new QNetworkAccessManager(this);

  QActionGroup *styleActionGroup = new QActionGroup(this);
  styleActionGroup->addAction(ui->action_light);
  styleActionGroup->addAction(ui->action_gray);  
  styleActionGroup->addAction(ui->action_dark);

  QActionGroup *langActionGroup = new QActionGroup(this);
  langActionGroup->addAction(ui->actionSystem);
  langActionGroup->addAction(ui->actionChinese);
  langActionGroup->addAction(ui->actionEnglish);

  const AppConfig& config = ConfigManager::instance().config();
  m_geminiClient = new GeminiClient(m_networkManager, this);
  m_recorderService = new AudioRecorderService(config, this);
  m_sherpaManager = new SherpaManager(this);
  m_sherpaInstaller = new SherpaInstaller(m_networkManager, this);
  m_transcriptionService = new TranscriptionService(m_networkManager, m_sherpaManager, m_geminiClient, config, this);
  m_termsManager = new TermsLibraryManager(this);
  m_updateManager = new UpdateManager(m_networkManager, this);

  m_workflow = new WorkflowManager(config, this);
  m_workflow->initialize(m_recorderService, m_transcriptionService, m_sherpaManager);

  // 初始化动画
  m_drawerAnimation = new QPropertyAnimation(ui->drawer_stack, "maximumHeight", this);
  m_drawerAnimation->setDuration(300);
  m_drawerAnimation->setEasingCurve(QEasingCurve::InOutQuart);

  m_sphereOverlay = new SphereOverlay();
  //m_sphereOverlay->setListening();
  //m_sphereOverlay->showAtBottomCenter();

  m_updateThread = QThread::create([this]() { m_updateManager->checkForUpdates();});

  initSystemTray();
  initialize();
  connection();
  onLoadConfig();
}
    
void MainWin::initialize() {
    ui->recording_equ_comb->clear();
    auto items = m_recorderService->availableMicrophones();
    ui->recording_equ_comb->addItems(items);
    QTimer::singleShot(0, [this]() {
        ui->recording_equ_comb->setCurrentIndex(0);
    });

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

    QStringList gemini_model_items;
    gemini_model_items << "gemini-3.1-flash-lite";
    gemini_model_items << "gemini-3-flash-preview";
    gemini_model_items << "gemini-flash-lite-latest";
    gemini_model_items << "gemini-2.5-flash";
    gemini_model_items << "gemini-2.5-pro";
    gemini_model_items << "gemini-1.5-flash";
    gemini_model_items << "gemini-1.5-pro";
    ui->gemini_model_comb->addItems(gemini_model_items);

    QStringList ai_engine_items;
    ai_engine_items << "Google Gemini API";
    ai_engine_items << "本地/自订 (OpenAI 兼容)";
    ui->ai_engine_comb->addItems(ai_engine_items);

    QStringList gemini_style_items;
    gemini_style_items << "智慧预设";
    gemini_style_items << "商务正式";
    gemini_style_items << "日常口語";
    gemini_style_items << "简洁扼要";
    gemini_style_items << "自订Prompt";
    ui->gemini_style_comb->addItems(gemini_style_items);

    QStringList translate_language_comb;
    translate_language_comb << "不翻译";
    translate_language_comb << "英文";
    translate_language_comb << "日文";
    translate_language_comb << "韩文";
    translate_language_comb << "繁体中文";  
    translate_language_comb << "简体中文";
    ui->translate_language_comb->addItems(translate_language_comb);

    ui->download_list_widget->setSherpaInstaller(m_sherpaInstaller);
    ui->gpu_backend_widget->setBackendInstaller(m_networkManager);
    ui->terms_widget->setTermsManager(m_termsManager);
}   

void MainWin::connection(){
    auto& configManager = ConfigManager::instance();

    // Basic Setting
	connect(ui->shortcut_edit, &ShortcutEdit::hotkeyActivated, this, &MainWin::onHotkeyPressed);

    // Local Recognition
    connect(ui->uninstall_sherpa_btn, &QPushButton::clicked, this, [this]() {
        m_sherpaInstaller->uninstallAll();
    });

	connect(ui->language_comb, &QComboBox::currentIndexChanged, this, [this](int index) {
		QString language = ui->language_comb->itemText(index);
		auto models = ModelRegistry::GetModelsByLanguage(language);
		ui->local_model_comb->clear();
		ui->local_model_comb->addItems(models);
	});

    connect(ui->download_model_btn, &QPushButton::clicked, this, [this]() {
        onSaveConfig();

        const AppConfig& config = ConfigManager::instance().config();
        QString repoId = config.sherpa.localModelRepoId;
        LOG_INFO(QString("Download %1...").arg(repoId));
        m_sherpaInstaller->installModel(repoId);
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
        GeminiClient::RequestParams params;
        params.aiEngine = config.gemini.aiEngineIndex;
        params.apiKey = config.gemini.apiKey;
        params.customUrl = config.gemini.apiUrl;
        params.model = config.gemini.model;
        params.style = config.gemini.geminiStyle;
        params.customPrompt = config.gemini.prompt;
        params.aiVocab = config.gemini.vocab;
        m_geminiClient->testConnection(params);
    });
    connect(ui->identification_log_btn, &QPushButton::clicked, this, [this](){ onToggleDrawer(0);});
    connect(ui->config_log_btn, &QPushButton::clicked, this, [this](){ onToggleDrawer(1); });
    connect(ui->download_list_btn, &QPushButton::clicked, this, [this]() { onToggleDrawer(2); });

    // 更新检查
    connect(ui->actionUpdate, &QAction::triggered, this, [this]() { m_updateThread->start(); });
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

	// Sherpa 模型安装器
    connect(m_sherpaInstaller, &SherpaInstaller::installGroupFinished,
        this, [this](const QString& repoId, bool success, const QString& msg) {
            if (success) {
                const AppConfig& config = ConfigManager::instance().config();
                int threads = config.sherpa.threads;
                bool useGpu = config.sherpa.useGpu;
                m_sherpaManager->loadModel(repoId, threads, useGpu);
            }
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
        appConfig.gemini.vocab = rules.aiVocab;

		// 热词表
        QString hotwords;
        for (const QString& w : rules.hotwords) hotwords += w + '\n';
        appConfig.sherpa.hotwords = hotwords;
		ConfigManager::instance().save();
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
    m_updateThread->start();
    QWidget::showEvent(event);
}

AppConfig MainWin::extractConfigFromUI() {
    AppConfig uiConfig;
    // 基础/全局设置 (Global/Misc)
    uiConfig.backend = static_cast<AsrBackendKind>(ui->identification_backend_comb->currentIndex());
    uiConfig.continuousMode = ui->autiomatic_monitoring_checkbox->isChecked();
    uiConfig.hotkey = ui->shortcut_edit->getShortCut();
    //uiConfig.autoStopEnabled = ui->autiomatic_mute_checkbox->isChecked(); 

    // 音频设置 (Audio Group)
    uiConfig.audio.deviceId = ui->recording_equ_comb->currentIndex();
    uiConfig.audio.deviceName = ui->recording_equ_comb->currentText();
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

    // Gemini/AI 设置 (Gemini Group)
    uiConfig.gemini.aiEngineIndex = ui->ai_engine_comb->currentIndex();
    uiConfig.gemini.model = ui->gemini_model_comb->currentText();
    uiConfig.gemini.geminiStyle = ui->gemini_style_comb->currentText();
    uiConfig.gemini.apiUrl = ui->api_url_edit->text();
    uiConfig.gemini.apiKey = ui->api_key_edit->text();
    uiConfig.gemini.targetLang = ui->translate_language_comb->currentText();
    uiConfig.gemini.vocab = ui->ai_vocabulary_edit->text();
    uiConfig.gemini.prompt = ui->custom_commands_edit->text(); 

    // 快捷键 (Hotkey)
    uiConfig.Language = Language;
    uiConfig.style = style;

	// 词典设置 (Terms Group)
    auto rules = m_termsManager->buildAllRules();
    uiConfig.replaceRules = rules.replaceRules;
    uiConfig.gemini.vocab = rules.aiVocab;

    QString hotwords;
    for (const QString& w : rules.hotwords) hotwords += w + '\n';
    uiConfig.sherpa.hotwords = hotwords;
    return uiConfig;
}

void MainWin::loadConfigToUI() {
    ConfigManager::instance().load();
    const AppConfig& cfg = ConfigManager::instance().config();
    // 基础/全局
    int audioIndex = ui->recording_equ_comb->findText(cfg.audio.deviceName);
    if (audioIndex != -1) ui->recording_equ_comb->setCurrentIndex(audioIndex);
    ui->autiomatic_monitoring_checkbox->setChecked(cfg.continuousMode);
    ui->shortcut_edit->setShortCut(cfg.hotkey);
	int backendIndex = static_cast<int>(cfg.backend);
	ui->identification_backend_comb->setCurrentIndex(backendIndex);
    //ui->autiomatic_mute_checkbox->setChecked(cfg.autoStopEnabled);
    
    // 音频
    ui->recording_equ_comb->setCurrentIndex(cfg.audio.deviceId);
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
		if (m_sherpaInstaller->isInstalled(cfg.sherpa.localModelRepoId)) {
			m_sherpaManager->loadModel(cfg.sherpa.localModelRepoId, cfg.sherpa.threads, cfg.sherpa.useGpu);
		}
    }

    // Gemini
    ui->ai_engine_comb->setCurrentIndex(cfg.gemini.aiEngineIndex);
    ui->gemini_model_comb->setCurrentText(cfg.gemini.model);
    ui->api_url_edit->setText(cfg.gemini.apiUrl);
    ui->api_key_edit->setText(cfg.gemini.apiKey);
    ui->ai_vocabulary_edit->setText(cfg.gemini.vocab);
    ui->custom_commands_edit->setText(cfg.gemini.prompt);
    ui->translate_language_comb->setCurrentText(cfg.gemini.targetLang);
    ui->gemini_style_comb->setCurrentText(cfg.gemini.geminiStyle);
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
    LOG_INFO(QString("Activated:" + ui->shortcut_edit->getShortCut()));
    if (m_workflow->currentState() == WorkflowState::Idle) {
        m_workflow->startRecording();
    }
    else {
        m_workflow->stopRecording();
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
    case WorkflowState::Processing:
        m_sphereOverlay->setTranscribe();
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
    if (pageIndex == ui->drawer_stack->currentIndex()) {
        m_drawerAnimation->setTargetObject(ui->drawer_stack);

        if (isTriggered) {
            isTriggered = false;
            m_drawerAnimation->setStartValue(ui->drawer_stack->maximumHeight());
            m_drawerAnimation->setEndValue(0);
        }
        else {
            isTriggered = true;
            m_drawerAnimation->setStartValue(ui->drawer_stack->maximumHeight());
            m_drawerAnimation->setEndValue(m_drawerHeight);
        }

        m_drawerAnimation->start();
    }
    else {
        ui->drawer_stack->setCurrentIndex(pageIndex);
    }
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



void MainWin::onUpdateFound(const QString& version, const QString& downloadUrl, const QString& notes) {
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("Update Available");
    msgBox.setText(QString("A new version (%1) is available!").arg(version));
    msgBox.setInformativeText("Would you like to download and install it now?");

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
    auto result = QMessageBox::question(this, "Restart Required",
        "Update downloaded successfully. The application will now close and restart to apply the update. Continue?",
        QMessageBox::Yes | QMessageBox::No);

    if (result == QMessageBox::Yes) {
        QString err;
        if (m_updateManager->launchUpdaterAndPrepareExit(filePath, &err)) {
            qApp->exit();
        }
        else {
            QMessageBox::critical(this, "Error", err);
            ui->actionUpdate->setEnabled(true);
        }
    }
    else {
        ui->actionUpdate->setEnabled(true);
    }
}

void MainWin::onLoadConfig()
{
    LOG_INFO("Load Configuration");
    loadConfigToUI();
    readIni();
}

void MainWin::onSaveConfig()
{
    LOG_INFO("Save Configuration");
    AppConfig uiConfig = extractConfigFromUI();
    ConfigManager::instance().updateConfig(uiConfig);
    if (ConfigManager::instance().save()) {
		LOG_INFO("Save Configuration Success");
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

