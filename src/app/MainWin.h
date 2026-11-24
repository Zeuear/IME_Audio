#ifndef MAINWIN_H
#define MAINWIN_H

#include <QMainWindow>
#include <QPropertyAnimation>
#include <QMessageBox>
#include <QSet>
#include <QHash>
#include <QSettings>
#include <QTranslator>
#include <qprocess.h>
#include <QSystemTrayIcon>
#include "ui/controls/LiveOverlay.h"
#include <QTableWidgetItem>
#include <QNetworkAccessManager>
#include <QElapsedTimer>
#include "services/WorkflowManager.h"
#include "utils/DownloadText.h"


class UpdateManager;
class OllamaClient;
class ConfigManager;
class AudioRecorderService;
class TranscriptionService; 
class WorkflowManager;
class TermsLibraryManager;
class SherpaInstaller;
class SherpaManager;
class LiveOverlay;
class ControlBar;
class LiveTranscriptWidget;
class ExpandPanel;
class QToolButton;
class ModelDownloadTracker;
class TermWidget;
class FileTranscribeWidget;
class LiveCaptionModel;
class SnippetStore;
class SnippetPaster;
class SnippetSuggestBar;
class SnippetSearchPopup;
class PrefixWatcher;
class StreamingEngine;
class TextPolishService;
class TranslationService;
class TranslationModelInstaller;
struct TranslationRoute;
class CudaInstaller;
class GpuRuntime;
class SettingsBinder;
class QTimer;
class DictationHistoryModel;
class DocsBridge;
class DocsSetupDialog;


namespace Ui {
class MainWin;
}

class MainWin : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWin(QWidget *parent = nullptr);
    ~MainWin() override;

    QTranslator mTranslator;

    void initialize();
    void setupUiConnections();
    void readIni();

protected:
    virtual void changeEvent(QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void showEvent(QShowEvent* event) override;

public slots:
    void onShowWindow();
    // 按上次的形态显示：横条或设置窗口（启动、再次启动程序时用）
    void showCurrentForm();

private slots:
    void onTrayIconActivated(QSystemTrayIcon::ActivationReason reason);
    void showControlBar();
    void hideControlBar();
    // 横条与设置窗口二选一，位置继承；见 ADR 0005
    void switchToBar();
    void switchToSettings();
    void toggleCurrentForm();
    void movePagesTo(bool settingsWindow);
    void refreshFormSwitchIcon();
    void onHideWindow();
    void onQuitApplication();
	void onNewLogEntry(const QString& logEntry);

private slots:
    void onUpdateFound(const QString& version, const QString& downloadUrl, const QString& notes);
    void onUpdateDownloaded(const QString& filePath);
    void onLoadConfig();
    void onHotkeyPressed();
    void onHotkeyReleased();
    void onStateChanged(WorkflowState newState);
    void onLocalModelActivated();
    // 切换 CPU/GPU：选中的模型不变，按模式加载对应版本；GPU 版没下载就自动下载，下完由安装器的加载信号重载
    void onComputeModeChanged(bool toGpu);
    void ensureGpuVariant(const QString& repoId);
    // 首次加载模型：useGpu 为真时等 GPU 检测结束再加载（最多 kGpuDetectionWaitMs），只加载一次
    void requestPreload();
    void settleGpuDetection(const char* reason);
    void onGpuDetectionFinished(bool ready);

private:
    void setupAppearanceUi();
    void retranslateAppearanceItems();
    void setTheme(LiveOverlay::Theme theme, bool persist);
    void setLanguage(const QString& key, bool persist);
    void syncAppearanceCombos();
    void fitToScreen();
    void alignSettingsCards();
    bool m_fittedToScreen = false;

    // 听写进行中锁住各设置页
    void setSettingsLocked(bool locked);

    enum class NotifyLevel { Info, Success, Warning, Error };
    void notify(NotifyLevel level, const QString& titleCN, const QString& causeCN = {});

    // 设置项登记表与生效处理
    void registerSettings();
    void applySettings(unsigned effects);
    void loadConfigToUI();
    void updateAiEngineUi();
    void placeModelRow(bool inPolishCard);
    void placeUrlRow(bool alone);
    void swapPolishModelForEngine(int engine);
    void retranslateDynamicItems();
    void rebuildTranslationLanguages();
    // 按当前引擎拉模型列表（OpenAI 兼容 / Ollama）；结果回到 modelsFetched
    void requestModelList();
    // 翻译模型下拉：首项"跟随润色模型"，其余为引擎里的模型；保留已保存但不在列表里的值
    void rebuildTranslateModelCombo(const QStringList& models);
    // 润色模型下拉：listed=false 表示没拉到列表（如服务没开），此时保留已保存的模型
    void rebuildPolishModelCombo(const QStringList& installed, bool listed);
    void updateTranslationUi();
    TranslationRoute currentTranslationRoute() const;
    QString recognitionLanguageCode() const;

    // 开始 / 停止听写
    bool startDictation();
    void setDictationIndicator(bool recording);
    void stopDictation();

    void setupLanguageCombo();
    void refreshLanguageComboTexts();
    QString currentLanguageKey() const;
    void initSystemTray();
    void rebuildTrayMenu();

    void syncLiveCaption();
    void updateSnippetSuggestions(const QString& spokenText);

    Ui::MainWin *ui;

    QSystemTrayIcon* trayIcon;
    QToolButton* m_formSwitchBtn = nullptr;   // 设置窗口标签栏右侧，切到横条
    QString m_liveShownText;                  // 刚补完译文的这句的最终文字，结果信号到了据此跳过重复追加
    bool m_liveShownValid = false;
    int m_lastAiEngine = -1;                  // 当前 polish.model 属于哪个引擎（见 swapPolishModelForEngine）
    bool m_urlRowAlone = false;               // 地址行是否独占一行（Ollama，见 placeUrlRow）
    bool m_modelRowInPolishCard = false;      // 润色模型行现在在哪张卡片里（见 placeModelRow）
    bool m_pagesInSettings = false;           // 词典 / 文件转录页现在挂在谁身上
    QMenu* trayMenu;

    QNetworkAccessManager* m_networkManager = nullptr;
    LiveOverlay* m_liveOverlay = nullptr;
    ControlBar* m_controlBar = nullptr;                 // 常驻置顶
    LiveTranscriptWidget* m_transcriptWidget = nullptr; // 稿子控件
    ModelDownloadTracker* m_downloadTracker = nullptr;  // 模型列表行内下载进度的状态
    ExpandPanel* m_panel = nullptr;                     // 实时转录 / 词典 / 文件转录
    TermWidget* m_termWidget = nullptr;                 // 词典页
    FileTranscribeWidget* m_fileWidget = nullptr;       // 文件转录页
    LiveCaptionModel* m_captionModel = nullptr;
    SnippetStore* m_snippetStore = nullptr;
    SnippetPaster* m_snippetPaster = nullptr;
    SnippetSuggestBar* m_suggestBar = nullptr;
    QSet<QString> m_handledSnippetIds;   
    SnippetSearchPopup* m_searchPopup = nullptr;
    PrefixWatcher* m_prefixWatcher = nullptr; 
    StreamingEngine* m_captionEngine = nullptr;
    bool m_liveCaptionFailNotified = false;

    TextPolishService* m_textPolishService = nullptr;
    OllamaClient* m_ollamaClient = nullptr;
    TranslationService* m_translationService = nullptr;
    TranslationModelInstaller* m_translationInstaller = nullptr;
    SherpaManager* m_sherpaManager = nullptr;
    SherpaInstaller* m_sherpaInstaller = nullptr;
    CudaInstaller* m_cudaInstaller = nullptr;
    GpuRuntime* m_gpuRuntime = nullptr;
    TermsLibraryManager* m_termsManager = nullptr;
    UpdateManager* m_updateManager = nullptr;
    AudioRecorderService *m_recorderService = nullptr;
    TranscriptionService *m_transcriptionService = nullptr;

    WorkflowManager *m_workflow = nullptr;
    SettingsBinder* m_settings = nullptr;
    QTimer* m_saveTimer = nullptr;   
    DocsBridge* m_docsBridge = nullptr;
    DocsSetupDialog* m_docsDialog = nullptr;
    QElapsedTimer m_docsWarnTimer;
    DictationHistoryModel* m_history = nullptr;   
    bool m_indicatorRecording = false;  
    bool m_loadingSettings = false;
    bool m_userCancelledInstall = false;
    QHash<QString, QString> m_installFailCause;   // repoId -> 解压失败原因，用于覆盖笼统的"检查网络"提示
    QString m_autoGpuRepoId;   // 切到 GPU 模式时自动下载中的 GPU 版仓库，空 = 没有
    static constexpr int kGpuDetectionWaitMs = 5000;
    QTimer* m_detectTimer = nullptr;
    bool m_preloadDeferred = false;     // 首次加载被推迟，等检测结果
    bool m_detectionTimedOut = false;
    DownloadGroupProgress m_translateProgress; 

    // 按住说话
    QElapsedTimer m_holdTimer;
    bool m_holdStarted = false;
    bool isTriggered = false;

    QString style;
    QString Language;
};

#endif 

