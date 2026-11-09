#ifndef MAINWIN_H
#define MAINWIN_H

#include <QMainWindow>
#include <QPropertyAnimation>
#include <QMessageBox>
#include <QSet>
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
class ConfigManager;
class AudioRecorderService;
class TranscriptionService; 
class WorkflowManager;
class TermsLibraryManager;
class SherpaInstaller;
class SherpaManager;
class LiveOverlay;
class ControlBar;
class TranscriptWindow;
class LiveTranscriptWidget;
class ExpandPanel;
class ModelDownloadTracker;
class TermWidget;
class FileTranscribeWidget;
class LiveCaptionModel;
class SnippetStore;
class SnippetPaster;
class SnippetSuggestBar;
class SnippetSearchPopup;
class PrefixWatcher;
class StreamingCaptionEngine;
class TextPolishService;
class TranslationService;
class TranslationModelInstaller;
struct TranslationRoute;
class CudaInstaller;
class SettingsBinder;
class QTimer;
class DictationHistoryModel;


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

private slots:
    void onTrayIconActivated(QSystemTrayIcon::ActivationReason reason);
    void showControlBar();
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
    void retranslateDynamicItems();
    void rebuildTranslationLanguages();
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
    QMenu* trayMenu;

    QNetworkAccessManager* m_networkManager = nullptr;
    LiveOverlay* m_liveOverlay = nullptr;
    ControlBar* m_controlBar = nullptr;                 // 常驻置顶
    TranscriptWindow* m_transcriptWindow = nullptr;     // 转录内容独立窗口
    LiveTranscriptWidget* m_transcriptWidget = nullptr; // 稿子控件
    ModelDownloadTracker* m_downloadTracker = nullptr;  // 模型列表行内下载进度的状态
    ExpandPanel* m_panel = nullptr;                     // 词典 / 文件转录
    TermWidget* m_termWidget = nullptr;                 // 词典页
    FileTranscribeWidget* m_fileWidget = nullptr;       // 文件转录页
    LiveCaptionModel* m_captionModel = nullptr;
    SnippetStore* m_snippetStore = nullptr;
    SnippetPaster* m_snippetPaster = nullptr;
    SnippetSuggestBar* m_suggestBar = nullptr;
    QSet<QString> m_handledSnippetIds;   
    SnippetSearchPopup* m_searchPopup = nullptr;
    PrefixWatcher* m_prefixWatcher = nullptr; 
    StreamingCaptionEngine* m_captionEngine = nullptr;  
    bool m_liveCaptionFailNotified = false;

    TextPolishService* m_textPolishService = nullptr;
    TranslationService* m_translationService = nullptr;
    TranslationModelInstaller* m_translationInstaller = nullptr;
    SherpaManager* m_sherpaManager = nullptr;
    SherpaInstaller* m_sherpaInstaller = nullptr;
    CudaInstaller* m_cudaInstaller = nullptr;
    TermsLibraryManager* m_termsManager = nullptr;
    UpdateManager* m_updateManager = nullptr;
    AudioRecorderService *m_recorderService = nullptr;
    TranscriptionService *m_transcriptionService = nullptr;

    WorkflowManager *m_workflow = nullptr;
    SettingsBinder* m_settings = nullptr;
    QTimer* m_saveTimer = nullptr;   
    DictationHistoryModel* m_history = nullptr;   
    bool m_indicatorRecording = false;  
    bool m_loadingSettings = false;
    bool m_userCancelledInstall = false;  
    DownloadGroupProgress m_translateProgress; 

    // 按住说话
    QElapsedTimer m_holdTimer;
    bool m_holdStarted = false;
    bool isTriggered = false;

    QString style;
    QString Language;
};

#endif 

