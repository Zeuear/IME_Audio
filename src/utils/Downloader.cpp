#include "Downloader.h"
#include <QDir>
#include <QFileInfo>
#include <QNetworkRequest>

Downloader::Downloader(QNetworkAccessManager* nam, QObject* parent) : QObject(parent), m_nam(nam)
{}

Downloader::~Downloader()
{
    cancel();
}

bool Downloader::isRunning() const
{
    return m_reply != nullptr;
}

void Downloader::start(const QUrl& url, const QString& savePath)
{
    cancel(); 

    m_savePath = savePath;
    m_tmpPath = savePath + ".part"; 
    m_fileName = QFileInfo(savePath).fileName();
    m_bytesReceived = 0;
    m_bytesTotal = -1;
    m_bytesAtLastTick = 0;

    QDir().mkpath(QFileInfo(savePath).absolutePath());

    m_file.setFileName(m_tmpPath);
    if (!m_file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        emit error("无法创建文件: " + m_tmpPath);
        return;
    }

    QNetworkRequest req(url);
    req.setAttribute(QNetworkRequest::CacheSaveControlAttribute, false);

    m_reply = m_nam->get(req);

    connect(m_reply, &QIODevice::readyRead, this, &Downloader::onReadyRead);
    connect(m_reply, &QNetworkReply::downloadProgress, this, &Downloader::onDownloadProgress);
    connect(m_reply, &QNetworkReply::finished, this, &Downloader::onFinished);
    connect(m_reply, &QNetworkReply::errorOccurred, this, &Downloader::onError);

    m_speedTimer.start();
}

void Downloader::cancel()
{
    if (m_reply) {
        // 断开信号，避免 abort() 触发的 finished/error 再被处理一次
        m_reply->disconnect(this);
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }
    if (m_file.isOpen()) {
        m_file.close();
    }
    // 取消时删除未下载完成的临时文件
    if (!m_tmpPath.isEmpty() && QFile::exists(m_tmpPath)) {
        QFile::remove(m_tmpPath);
    }
}

void Downloader::cleanupReply()
{
    if (m_reply) {
        m_reply->deleteLater();
        m_reply = nullptr;
    }
}

void Downloader::failAndCleanup(const QString& msg)
{
    if (m_file.isOpen()) {
        m_file.close();
    }
    if (!m_tmpPath.isEmpty() && QFile::exists(m_tmpPath)) {
        QFile::remove(m_tmpPath);
    }
    cleanupReply();
    emit error(msg);
}

void Downloader::onReadyRead()
{
    if (!m_reply || !m_file.isOpen())
        return;

    // 每次有新数据到达就立刻写入磁盘，内存中只保留当前这一小块数据
    const QByteArray chunk = m_reply->readAll();
    if (!chunk.isEmpty()) {
        qint64 written = m_file.write(chunk);
        if (written != chunk.size()) {
            failAndCleanup("写入文件失败: " + m_tmpPath);
            if (m_reply) m_reply->abort();
            return;
        }
        m_bytesReceived += written;
    }
}

void Downloader::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    m_bytesTotal = bytesTotal;

    double speed = 0.0;
    qint64 elapsedMs = m_speedTimer.elapsed();
    if (elapsedMs > 0) {
        qint64 deltaBytes = bytesReceived - m_bytesAtLastTick;
        speed = deltaBytes * 1000.0 / elapsedMs;
    }
    m_bytesAtLastTick = bytesReceived;
    m_speedTimer.restart();

    emit progress(m_fileName, bytesReceived, bytesTotal, speed);
}

void Downloader::onFinished()
{
    if (!m_reply)
        return;

    if (m_reply->error() == QNetworkReply::NoError) {
        const QByteArray remain = m_reply->readAll();
        if (!remain.isEmpty()) {
            m_file.write(remain);
            m_bytesReceived += remain.size();
        }
        m_file.close();

        if (QFile::exists(m_savePath)) {
            QFile::remove(m_savePath);
        }
        if (!QFile::rename(m_tmpPath, m_savePath)) {
            cleanupReply();
            emit error("重命名文件失败: " + m_tmpPath + " -> " + m_savePath);
            return;
        }

        cleanupReply();
        emit finished(m_savePath);
    }
    else {
        cleanupReply();
    }
}

void Downloader::onError(QNetworkReply::NetworkError code)
{
    Q_UNUSED(code)
    QString err = m_reply ? m_reply->errorString() : "未知错误";
    failAndCleanup(err);
}