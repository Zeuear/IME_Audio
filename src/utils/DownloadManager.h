#pragma once
#include <QObject>
#include <QQueue>
#include <QList>
#include <QMap>
#include "Downloader.h"
#include "DownloadTask.h"

struct DownloadGroup {
    QString groupId;
    QString displayName;     
    int totalFiles = 0;
    int finishedFiles = 0;
    int failedFiles = 0;
    QHash<QString, qint64> fileReceived; 
    QHash<QString, qint64> fileTotal;  
};


class DownloadManager : public QObject
{
    Q_OBJECT
public:
	static DownloadManager& instance(QNetworkAccessManager* nam, QObject* parent = nullptr) {
		static DownloadManager instance(nam, parent);
		return instance;
	}

    explicit DownloadManager(QNetworkAccessManager* nam, QObject* parent = nullptr);
    ~DownloadManager();

    QString addTask(const QUrl& url, const QString& savePath, TaskFinishedCallback onComplete, TaskErrorCallback onError);
    void cancelTask(const QString& taskId);
    void cancelAll();

	// Group download management
    QString addGroupTask(const QString& groupId, const QString& displayName,
                         const QUrl& url, const QString& savePath,
                         TaskFinishedCallback onComplete = nullptr, TaskErrorCallback onError = nullptr);
    void cancelGroup(const QString& groupId);


    QList<DownloadTask*> allTasks() const { return m_tasks; }
    QList<DownloadTask*> tasksInGroup(const QString& groupId) const;

signals:
    void taskAdded(DownloadTask* task);
    void taskProgress(const QString& taskId, qint64 received, qint64 total, double speed);
    void taskFinished(const QString& taskId, const QString& savePath);
    void taskError(const QString& taskId, const QString& error);
    void queueChanged(); 


    void groupStarted(const QString& groupId, const QString& displayName, int totalFiles);
    void groupFileProgress(const QString& groupId, const QString& taskId, const QString& filename,
                            qint64 received, qint64 total, int overallPercent);
    void groupFileFinished(const QString& groupId, const QString& taskId, const QString& filename);
    void groupFileError(const QString& groupId, const QString& taskId, const QString& filename, const QString& error);
    void groupFinished(const QString& groupId, bool success);

private slots:
    void onDownloaderFinished(const QString& savePath);
    void onDownloaderError(const QString& error);
    void onDownloaderProgress(const QString& fileName, qint64 received, qint64 total, double speed);

private:
    void startNext();
    DownloadTask* findActiveTaskBySavePath(const QString& savePath) const;
    void updateGroupOnProgress(DownloadTask* task, qint64 received, qint64 total);
    void updateGroupOnFinished(DownloadTask* task);
    void updateGroupOnError(DownloadTask* task, const QString& error);
    void maybeEmitGroupFinished(const QString& groupId);

    QNetworkAccessManager* m_nam;
    QList<DownloadTask*> m_tasks;        
    QQueue<DownloadTask*> m_queue;       
    QMap<QString, DownloadTask*> m_activeMap;
    QHash<QString, DownloadGroup> m_groups;

    Downloader* m_currentDownloader = nullptr;
    int m_maxConcurrent = 1; 
    int m_runningCount = 0;
};