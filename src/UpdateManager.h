#ifndef UPDATEMANAGER_H
#define UPDATEMANAGER_H

#include <QObject>
#include <QString>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

class QSaveFile;

class UpdateManager : public QObject {
    Q_OBJECT
public:
    explicit UpdateManager(QNetworkAccessManager* networkManager, QObject* parent = nullptr);

    // 设置项目信息
    void setProjectInfo(const QString& owner, const QString& repo, const QString& currentVersion);

    // 开始检查更新
    void checkForUpdates();

    // 下载更新到指定路径
    void downloadUpdate(const QString& savePath);

    bool launchUpdaterAndPrepareExit(const QString& downloadedFilePath, QString* errorMessage = nullptr);

    bool launchUpdaterMacOS(const QString& downloadedFilePath, QString* errorMessage);
    bool launchUpdaterLinux(const QString& downloadedFilePath, QString* errorMessage);
    bool launchUpdaterWindows(const QString& downloadedFilePath, QString* errorMessage);

signals:
    void updateAvailable(const QString& version, const QString& downloadUrl, const QString& releaseNotes);
    void updateError(const QString& error);
    void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void downloadFinished(const QString& filePath);

private slots:
    void onVersionCheckFinished(QNetworkReply* reply);
    void onDownloadFinished(QNetworkReply* reply, const QString& savePath, QSaveFile* file);

private:
    QString m_owner;
    QString m_repo;
    QString m_currentVersion;
    QString m_downloadUrl;
    QString m_releaseNotes;

    QNetworkAccessManager* m_networkManager;
};

#endif