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
#include "AppConfig.h"
#include "WorkflowManager.h"

class UpdateManager;
class ConfigManager;
class AudioRecorderService;
class TranscriptionService; 
class WorkflowManager;
class TermsLibraryManager;
class SherpaInstaller;
class SherpaManager;
class SphereOverlay;
class GeminiClient;
class CudaInstaller;


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
    void connection();
    void readIni();

protected:
    virtual void changeEvent(QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void showEvent(QShowEvent* event) override;


private slots:
    void onTrayIconActivated(QSystemTrayIcon::ActivationReason reason);
    void onShowWindow();
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

    void onUpdateFound(const QString& version, const QString& downloadUrl, const QString& notes);
    void onUpdateDownloaded(const QString& filePath);

    void onLoadConfig();
    void onSaveConfig();

    void onHotkeyPressed();
    void onStateChanged(WorkflowState newState);

private:
    AppConfig extractConfigFromUI();
    void loadConfigToUI();
    void initSystemTray();

    Ui::MainWin *ui;

    QSystemTrayIcon* trayIcon;
    QMenu* trayMenu;
    QAction* showAction;
    QAction* hideAction;
    QAction* quitAction;
    QAction* separatorAction;

    QNetworkAccessManager* m_networkManager = nullptr;
    SphereOverlay* m_sphereOverlay = nullptr;

    GeminiClient* m_geminiClient = nullptr;
    SherpaManager* m_sherpaManager = nullptr;
    SherpaInstaller* m_sherpaInstaller = nullptr;
    CudaInstaller* m_cudaInstaller = nullptr;
    TermsLibraryManager* m_termsManager = nullptr;
    UpdateManager* m_updateManager = nullptr;
    AudioRecorderService *m_recorderService = nullptr;
    TranscriptionService *m_transcriptionService = nullptr;

    WorkflowManager *m_workflow = nullptr;
    QPropertyAnimation *m_drawerAnimation;
    bool isTriggered = false;

    QString style;
    QString Language;
};

#endif 