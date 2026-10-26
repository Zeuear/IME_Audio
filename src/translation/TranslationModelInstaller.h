#pragma once

#include "LanguageRouting.h"
#include "RemoteSettingsIndex.h"

#include <QMap>
#include <QObject>
#include <QSet>

class DownloadManager;
class QNetworkAccessManager;

// @brief 按方向下载、校验、安装本地翻译模型；一个方向一个下载组，经英语中转的路线是两个组。
// 信号形状与 CudaInstaller 一致，可以直接接到下载列表页。
class TranslationModelInstaller : public QObject {
    Q_OBJECT
public:
    TranslationModelInstaller(QNetworkAccessManager* nam, const QString& modelsRoot,
                              QMap<QString, DirectionManifest> manifests, QObject* parent = nullptr);

    // 清单里已有的方向，元素形如 "zh-Hans>en"，供 planRoute 使用
    QSet<QString> knownPairs() const;

    bool isInstalled(const QString& from, const QString& to) const;
    bool isInstalling(const QString& from, const QString& to) const;
    bool isRouteInstalled(const TranslationRoute& route) const;
    qint64 bytesToDownload(const TranslationRoute& route) const;

    void install(const TranslationRoute& route);

    QString writeConfigFile(const QString& from, const QString& to) const;

signals:
    void installGroupStarted(const QString& groupId, const QString& displayName, int totalFiles);
    void installFileProgress(const QString& groupId, const QString& filename, qint64 received, qint64 total, int overallPercent);
    void installFileFinished(const QString& groupId, const QString& filename);
    void installFileError(const QString& groupId, const QString& filename, const QString& error);
    void installGroupFinished(const QString& groupId, bool success, const QString& message);

    void directionInstalled(const QString& from, const QString& to);
    void installFailed(const QString& from, const QString& to, const QString& cause);

private slots:
    void onGroupFinished(const QString& groupId, bool success);

private:
    static QString groupIdFor(const QString& from, const QString& to);
    void installDirection(const DirectionManifest& manifest);
    void discardPartial(const DirectionManifest& manifest) const;

    DownloadManager* m_downloads = nullptr;
    QString m_modelsRoot;
    QMap<QString, DirectionManifest> m_manifests;
    QSet<QString> m_installing;   // 下载中的 groupId
};
