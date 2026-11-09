#pragma once
#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QFile>
#include <QUrl>
#include <QElapsedTimer>
#include <QTimer>
#include <functional>

class Downloader : public QObject
{
    Q_OBJECT
public:
    explicit Downloader(QNetworkAccessManager* nam, QObject* parent = nullptr);
    ~Downloader();

    void start(const QUrl& url, const QString& savePath);
    void cancel();
    bool isRunning() const;

    // 可选设置，需在 start() 之前调用。
    void setUserAgent(const QByteArray& ua) { m_userAgent = ua; }
    // 超过该时间没有收到任何数据就中止并报错；0 表示不限
    void setStallTimeoutMs(int ms) { m_stallTimeoutMs = ms; }
    // 下载完成、替换目标文件之前的校验：返回空串表示通过，否则作为错误信息，临时文件会被丢弃
    void setVerifier(std::function<QString(const QString& partPath)> verifier) { m_verifier = std::move(verifier); }

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

    QByteArray m_userAgent;
    int m_stallTimeoutMs = 0;
    QTimer m_stallTimer;
    std::function<QString(const QString&)> m_verifier;

    QElapsedTimer m_speedTimer;
    qint64 m_bytesAtLastTick = 0;
};