#ifndef MAINWIN_H
#define MAINWIN_H

#include <QMainWindow>
#include <QPropertyAnimation>
#include <QMessageBox>
#include <QSettings>
#include <QTranslator>
#include <qprocess.h>
#include <QSystemTrayIcon>
#include <QTableWidgetItem>
#include <QNetworkAccessManager>
#include <QElapsedTimer>
#include "WorkflowManager.h"


class UpdateManager;
class ConfigManager;
class AudioRecorderService;
class TranscriptionService; 
class WorkflowManager;
class TermsLibraryManager;
class SherpaInstaller;
class SherpaManager;
class LiveOverlay;
class LiveCaptionModel;
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
    // 也用于响应"用户又启动了一份程序"：把已在运行的窗口调到前台
    void onShowWindow();

private slots:
    void onTrayIconActivated(QSystemTrayIcon::ActivationReason reason);
    void onHideWindow();
    void onQuitApplication();

    void on_actionAbout_Qt_triggered();
    void on_action_light_triggered();
    void on_action_gray_triggered();
    void on_action_dark_triggered();
    void on_actionSystem_triggered();
    void on_actionChinese_triggered();
    void on_actionEnglish_triggered();
    void on_actionExit_triggered();

    void onToggleDrawer(int pageIndex);
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
    void fitToScreen();
    void alignSettingsCards();
    bool m_fittedToScreen = false;

    // 听写进行中锁住各设置页和保存/取消
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

    Ui::MainWin *ui;

    QSystemTrayIcon* trayIcon;
    QMenu* trayMenu;

    QNetworkAccessManager* m_networkManager = nullptr;
    LiveOverlay* m_liveOverlay = nullptr;
    LiveCaptionModel* m_captionModel = nullptr;
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
    QTimer* m_saveTimer = nullptr;   // 设置改动合并后再写文件
    DictationHistoryModel* m_history = nullptr;   
    bool m_indicatorRecording = false;   // 录音状态指示当前显示的状态，换语言后用来恢复文字
    bool m_loadingSettings = false;

    // 按住说话
    QElapsedTimer m_holdTimer;
    bool m_holdStarted = false;
    QPropertyAnimation *m_drawerAnimation;
    bool isTriggered = false;

    QString style;
    QString Language;
};

#endif 

