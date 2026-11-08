#pragma once
#include <QObject>
#include <QProcess>
#include <QString>
#include <QQueue>
#include <QSet>
#include <QUrl>
#include <QFileInfo>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <memory>
#include <functional>
#include <variant>

#include "config/AppConfig.h"
#include "utils/DownloadManager.h"
#include "utils/ProcessManager.h"
#include "services/interfaces/workflow_interfaces.h"
#include "SherpaPunctuator.h"
#include "SherpaConfig.h"


class SherpaManager : public ISherpaModel
{
    Q_OBJECT
public:
    explicit SherpaManager(QObject* parent = nullptr);
    ~SherpaManager();

    void loadModel(const AppConfig& config, bool isReload = false);
    bool reloadModel(const AppConfig& config);
    void unloadModel();

    // 空闲卸载模型计时控制
    void pauseIdleTimer();
    void resumeIdleTimer();

    bool isModelLoaded() const;
    bool isBusy() const;       
    int  pendingCount() const;   

public slots:
    void loadModelAsync(const AppConfig& config, bool isReload);
    void transcribeAsync(const QByteArray& pcmData, int sampleRate);
    void shutdown(); 
    void workerLoop(); 

signals:
    void utteranceTranscribed(bool success, const QString& text, const QString& errorMsg);
    void queueSizeChanged(int pendingCount);
    void gpuFallbackToCpu();
    void idleUnloaded();

private:
    void armIdleTimer();
    bool transcribeSync(const QByteArray& pcmData, int sampleRate, QString* outText, QString* outError);
    bool transcribeOffline(const std::vector<float>& samples, int sampleRate, QString* outText, QString* outError);
    bool transcribeOnline(const std::vector<float>& samples, int sampleRate, QString* outText, QString* outError);

    enum class TaskType { Transcribe, Load, Unload };

    struct PendingTask {
        // Transcribe
        TaskType type;
        QByteArray pcmData;  
        int sampleRate = 0;   

        // Load
        AppConfig config;   
        bool isReload = false;
    };

    RecognizerKind m_kind = RecognizerKind::None;
    std::unique_ptr<sherpa_onnx::cxx::OfflineRecognizer> m_offlineRecognizer;
    std::unique_ptr<sherpa_onnx::cxx::OnlineRecognizer>  m_onlineRecognizer;

    SherpaPunctuator* m_punctuator = nullptr;
    std::atomic<bool> m_isLoaded{ false };
    std::atomic<bool> m_busyFlag{ false };
    std::atomic<bool> m_stopWorker{ false };

    QString m_currentRepoId;
    AppConfig m_configCopy;

    // 空闲自动卸载
    static constexpr int kIdleUnloadMs = 30 * 60 * 1000;
    QTimer* m_idleTimer = nullptr;
    std::atomic<bool> m_idlePaused{ false };

    mutable QMutex m_recognizerMutex; 

    // 任务队列相关
    QQueue<PendingTask> m_queue;
    int m_loadsInFlight = 0;     
    AppConfig m_lastLoadRequest; 
    mutable QMutex m_queueMutex;
    QWaitCondition m_queueNotEmpty;

    QThread* m_workerThread;

};

class SherpaInstaller : public QObject
{
    Q_OBJECT
public:
    explicit SherpaInstaller(QNetworkAccessManager* nam, QObject* parent = nullptr);
    void uninstallAll();
    void installModel(const QString& repoId);
    // 取消正在下载的模型：停下载、删掉已下的残留。解压阶段不能取消，调用也不会有效果
    void cancelInstall(const QString& repoId);
    void uninstallModel(const QString& repoId);
    void ensurePunctModel(const QString& repoId);
    void ensureStreamingModel(const QString& language);

    // VAD 模型。
    void ensureVadModel();

    bool isInstalling(const QString& repoId) const;
    static bool isInstalled(const QString& repoId);

signals:
    void installationProgress(const QString& msg);
    void installationFinished(bool ok, const QString& msg);
    
    void installGroupStarted(const QString& repoId, const QString& displayName, int totalFiles);
    void installGroupFinished(const QString& repoId, bool success, const QString& msg);
    // 用户主动取消（先于下载层报告"组失败"发出，界面据此不把它当成失败）
    void installCancelled(const QString& repoId);

    void installFileProgress(const QString& repoId, const QString& filename, qint64 recv, qint64 total, int overallPercent);
    void installFileFinished(const QString& repoId, const QString& filename);
    void installFileError(const QString& repoId, const QString& filename, const QString& error);

    // 解压进度
    void extractStarted(const QString& repoId, int totalLines);
    void extractProgress(const QString& repoId, int current, int total);
    void extractFinished(const QString& repoId, bool success);

    void loadModel(const QString& repoId, bool success, const QString& msg);

    // 伴随模型就绪：不走 loadModel，否则会误触发主模型预热和"模型加载成功"提示
    void streamingModelReady(const QString& repoId);

    void vadModelReady(bool success, const QString& msg);

private slots:
    void onGroupFileProgress(const QString& groupId, const QString& taskId, const QString& filename,
                            qint64 received, qint64 total, int overallPercent);
    void onGroupFileFinished(const QString& groupId, const QString& taskId, const QString& filename);
    void onGroupFileError(const QString& groupId, const QString& taskId, const QString& filename, const QString& error);
    void onGroupFinished(const QString& groupId, bool success);
    void onSherpaExtractFinished(const QString& groupId, bool ok);

private:
    DownloadManager* m_downloadManager = nullptr;
    QHash<QString, ModelInstallManifest> m_activeManifests;
    QSet<QString> m_companionGroups;
    TaskQueueManager* m_extractQueue = nullptr;
};


struct DependencyResource {
    QString archiveName; 
    QUrl url;
    bool isCuda;
};

class SherpaDependencyManager : public QObject
{
    Q_OBJECT
public:
    explicit SherpaDependencyManager(QNetworkAccessManager* nam, QObject* parent = nullptr);
    void checkAndRepairDependencies(const QString& targetDir, bool useCuda, Downloader* downloader);

signals:
    void progress(const QString& msg, int percent);
    void finished(bool success, const QString& message);

private slots:
    void onDownloadFinished(const QString& path);
    void onDownloadError(const QString& err);

private:
    void startNext();

    QString m_targetDir;
    Downloader* m_downloader = nullptr;
    QList<DependencyResource> m_queue;
    int m_currentIndex = 0;
    bool m_isCuda = false;
};
