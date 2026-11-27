#ifndef UPDATEMANAGER_H
#define UPDATEMANAGER_H

#include <QObject>
#include <QString>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

class Downloader;

class UpdateManager : public QObject {
    Q_OBJECT
public:
    explicit UpdateManager(QNetworkAccessManager* networkManager, QObject* parent = nullptr);

    void setProjectInfo(const QString& owner, const QString& repo, const QString& currentVersion);

    void checkForUpdates();

    static QString defaultDownloadPath();

    // latest 是否比 current 新：忽略开头的 v，按版本号数值比；解析不出版本号时按字符串比（和原来一致）
    static bool isNewer(const QString& latest, const QString& current);

    void downloadUpdate(const QString& savePath);

    bool launchUpdaterAndPrepareExit(const QString& downloadedFilePath, QString* errorMessage = nullptr);

    bool launchUpdaterMacOS(const QString& downloadedFilePath, QString* errorMessage);
    bool launchUpdaterLinux(const QString& downloadedFilePath, QString* errorMessage);
    bool launchUpdaterWindows(const QString& downloadedFilePath, QString* errorMessage);

signals:
    void updateAvailable(const QString& version, const QString& downloadUrl, const QString& releaseNotes);
    void upToDate(const QString& currentVersion);
    void updateError(const QString& error);
    void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void downloadFinished(const QString& filePath);

private slots:
    void onVersionCheckFinished(QNetworkReply* reply);

private:
    QString m_owner;
    QString m_repo;
    QString m_currentVersion;
    QString m_downloadUrl;
    QString m_releaseNotes;

    QNetworkAccessManager* m_networkManager;
    Downloader* m_downloader = nullptr;
};

#endif