#pragma once
#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QFile>
#include <QUrl>
#include <QElapsedTimer>

class Downloader : public QObject
{
    Q_OBJECT
public:
    explicit Downloader(QNetworkAccessManager* nam, QObject* parent = nullptr);
    ~Downloader();

    void start(const QUrl& url, const QString& savePath);
    void cancel();
    bool isRunning() const;

signals:
    // received/total 为已接收/总大小；speedBytesPerSec 为估算的下载速度(字节/秒)
    void progress(const QString& fileName, qint64 received, qint64 total, double speedBytesPerSec);
    void finished(const QString& savePath);
    void error(const QString& errorString);

private slots:
    void onReadyRead();
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onFinished();
    void onError(QNetworkReply::NetworkError code);

private:
    void cleanupReply();
    void failAndCleanup(const QString& msg);

    QNetworkAccessManager* m_nam;
    QNetworkReply* m_reply = nullptr;
    QFile m_file;
    QString m_savePath;
    QString m_tmpPath;      // 下载过程中的临时文件路径 (savePath + ".part")
    QString m_fileName;

    qint64 m_bytesReceived = 0;
    qint64 m_bytesTotal = -1;

    QElapsedTimer m_speedTimer;
    qint64 m_bytesAtLastTick = 0;
};