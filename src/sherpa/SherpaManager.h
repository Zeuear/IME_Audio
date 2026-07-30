#pragma once
#include <QObject>
#include <QProcess>
#include <QString>
#include <QQueue>
#include <QUrl>
#include <QFileInfo>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <memory>
#include <functional>
#include <variant>
#include "cxx-api.h"

#include "utils/DownloadManager.h"
#include "SherpaConfig.h"



class SherpaManager : public QObject
{
    Q_OBJECT
public:
    explicit SherpaManager(QObject* parent = nullptr);
    ~SherpaManager();

    void loadModel(const QString& repoId, int numThreads = 4, bool useGpu = false);
    void unloadModel();
    bool isModelLoaded() const;

    bool transcribeSync(const QByteArray& pcmData, int sampleRate, QString* outText, QString* outError);
    
    bool isBusy() const;       
    int  pendingCount() const;   

public slots:
    void transcribeAsync(const QByteArray& pcmData, int sampleRate);
    void shutdown(); 
    void workerLoop(); 

signals:
    void utteranceTranscribed(bool success, const QString& text, const QString& errorMsg);
    void queueSizeChanged(int pendingCount); 

private:
    bool transcribeOffline(const std::vector<float>& samples, int sampleRate, QString* outText, QString* outError);
    bool transcribeOnline(const std::vector<float>& samples, int sampleRate, QString* outText, QString* outError);

    struct PendingUtterance {
        QByteArray pcmData;
        int sampleRate;
    };

    RecognizerKind m_kind = RecognizerKind::None;
    std::unique_ptr<sherpa_onnx::cxx::OfflineRecognizer> m_offlineRecognizer;
    std::unique_ptr<sherpa_onnx::cxx::OnlineRecognizer>  m_onlineRecognizer;

    bool m_isLoaded = false;
    QString m_currentRepoId;

    mutable QMutex m_recognizerMutex; 

    // 任务队列相关
    QQueue<PendingUtterance> m_queue;
    mutable QMutex m_queueMutex;
    QWaitCondition m_queueNotEmpty;
    bool m_busyFlag = false;   
    bool m_stopWorker = false;

    QThread* m_workerThread;

};

class SherpaInstaller : public QObject
{
    Q_OBJECT
public:
    explicit SherpaInstaller(QNetworkAccessManager* nam, QObject* parent = nullptr);
    void uninstallAll();
    void installModel(const QString& repoId);

    bool isInstalling(const QString& repoId) const;
    bool isInstalled(const QString& repoId) const;

signals:
    void installationProgress(const QString& msg);
    void installationFinished(bool ok, const QString& msg);
    
    void installGroupStarted(const QString& repoId, const QString& displayName, int totalFiles);
    void installGroupFinished(const QString& repoId, bool success, const QString& msg);

    void installFileProgress(const QString& repoId, const QString& filename, qint64 recv, qint64 total, int overallPercent);
    void installFileFinished(const QString& repoId, const QString& filename);
    void installFileError(const QString& repoId, const QString& filename, const QString& error);

    void loadModel(const QString& repoId, bool success, const QString& msg);

private slots:
    void onGroupFileProgress(const QString& groupId, const QString& taskId, const QString& filename,
                            qint64 received, qint64 total, int overallPercent);
    void onGroupFileFinished(const QString& groupId, const QString& taskId, const QString& filename);
    void onGroupFileError(const QString& groupId, const QString& taskId, const QString& filename, const QString& error);
    void onGroupFinished(const QString& groupId, bool success);

private:
    DownloadManager* m_downloadManager = nullptr;
    QHash<QString, ModelInstallManifest> m_activeManifests;

    QString SHERPA_RUNTIME = "Sherpa Runtime";

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
