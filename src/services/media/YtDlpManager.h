#pragma once
#include <QHash>
#include <QList>
#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;
class Downloader;
class QProcess;
class QTimer;

// @brief
// 链接下载：按需获取并维护 yt-dlp，用它把公开链接里的音频下载到本地临时目录，一次一个。
// 第一次添加链接时才创建；组件缺失、联网失败、下载失败都只变成失败信号；各阶段都有超时。
// 组件只从官方 GitHub Release 下载，并用同一 Release 的 SHA2-256SUMS 校验。
class YtDlpManager : public QObject {
    Q_OBJECT
public:
    explicit YtDlpManager(QNetworkAccessManager* nam, QObject* parent = nullptr);
    ~YtDlpManager() override;

    static bool isPlatformSupported();

    quint64 enqueue(const QString& url);
    void cancel(quint64 id);
    void cancelAll();
    // 删除临时目录
    void release(quint64 id);

signals:
    void toolStatus(const QString& text);
    void taskStarted(quint64 id);
    void taskProgress(quint64 id, int percent);
    void taskTitle(quint64 id, const QString& title);
    void taskFinished(quint64 id, bool ok, const QString& filePath, const QString& title, const QString& error);

private:
    enum class Phase { Idle, Preparing, Running };

    struct Job {
        quint64 id = 0;
        QString url;
        bool retried = false;  
    };

    QString toolPath() const;
    bool toolIsFresh() const;
    void touchCheckStamp() const;
    void pump();
    void prepareTool(bool force);
    void fetchSums(bool toolExists);
    void downloadTool(const QString& expectedSha);
    void toolReady();
    void failPreparation(const QString& error);
    QNetworkReply* get(const QString& url, int timeoutMs);

    void startJob();
    void onProcessFinished(int exitCode);
    void failCurrent(const QString& error);
    void finishCurrentSuccess(const QString& title, const QString& path);
    void cleanupCurrent();

    QNetworkAccessManager* m_nam = nullptr;
    Phase m_phase = Phase::Idle;
    QList<Job> m_queue;
    quint64 m_nextId = 1;
    bool m_forceUpdate = false;

    Job m_current;
    QProcess* m_process = nullptr;
    QTimer* m_watchdog = nullptr;
    QString m_stderr;
    QString m_workDir;
    bool m_cancelled = false;
    bool m_timedOut = false;

    QHash<quint64, QString> m_workDirs; 
    Downloader* m_toolDownloader = nullptr;
};
