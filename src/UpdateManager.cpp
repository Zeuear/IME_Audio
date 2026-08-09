#include "UpdateManager.h"
#include <QFile>
#include <QSaveFile>
#include <QDir>
#include <QDebug>
#include <QCoreApplication>
#include <QFileInfo>
#include <QVersionNumber>
#include <QProcess>
#include <QStandardPaths>
#include "utils/Logger.h"
#include "version.h"


UpdateManager::UpdateManager(QNetworkAccessManager* networkManager, QObject* parent)
    : QObject(parent), m_networkManager(networkManager) {
    setProjectInfo("Zeuear", "IME_Audio", PROJECT_VERSION);
}

void UpdateManager::setProjectInfo(const QString& owner, const QString& repo, const QString& currentVersion) {
    m_owner = owner;
    m_repo = repo;
    m_currentVersion = currentVersion;
}

void UpdateManager::checkForUpdates() {
    if (m_owner.isEmpty() || m_repo.isEmpty()) {
        emit updateError("Project info not set.");
        return;
    }

    m_downloadUrl.clear();
    m_releaseNotes.clear();

    QString url = QString("https://api.github.com/repos/%1/%2/releases/latest").arg(m_owner, m_repo);
    QNetworkRequest request((QUrl(url)));
    request.setHeader(QNetworkRequest::UserAgentHeader, "VoiceIME-Updater-Client");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply* reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        onVersionCheckFinished(reply);
    });
}

void UpdateManager::onVersionCheckFinished(QNetworkReply* reply) {
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        emit updateError(reply->errorString());
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    QJsonObject obj = doc.object();

    if (obj.isEmpty()) {
        emit updateError("Invalid response from GitHub.");
        return;
    }

    QString latestVersion = obj.value("tag_name").toString().trimmed();
    if (latestVersion.isEmpty()) {
        emit updateError("Release response missing tag_name.");
        return;
    }

    auto stripV = [](QString v) {
        if (v.startsWith('v', Qt::CaseInsensitive)) v.remove(0, 1);
        return v;
        };
    QVersionNumber latestNum = QVersionNumber::fromString(stripV(latestVersion));
    QVersionNumber currentNum = QVersionNumber::fromString(stripV(m_currentVersion));

    if (latestNum.isNull() || currentNum.isNull()) {
        LOG_DEBUG(QString("Could not parse version numbers, falling back to string compare.").arg(latestVersion).arg(m_currentVersion));
        if (latestVersion <= m_currentVersion) {
            LOG_INFO("当前版本为最新");
            LOG_DEBUG(QString("Current version is up to date:").arg(m_currentVersion));
            return;
        }
    }
    else if (latestNum <= currentNum) {
        LOG_INFO("当前版本为最新");
        LOG_DEBUG(QString("Current version is up to date:").arg(m_currentVersion));
        return;
    }

    QJsonArray assets = obj.value("assets").toArray();
    QString expectedSuffix;
#if defined(Q_OS_WIN)
    expectedSuffix = ".exe";
#elif defined(Q_OS_MAC)
    expectedSuffix = ".dmg";
#elif defined(Q_OS_LINUX)
    expectedSuffix = ".AppImage";
#endif

    QString downloadUrl;
    for (int i = 0; i < assets.size(); ++i) {
        QString assetName = assets[i].toObject().value("name").toString();
        if (!expectedSuffix.isEmpty() && assetName.endsWith(expectedSuffix, Qt::CaseInsensitive)) {
            downloadUrl = assets[i].toObject().value("browser_download_url").toString();
            break;
        }
    }

    if (downloadUrl.isEmpty()) {
        emit updateError(tr("Could not find an executable asset in the latest release."));
        return;
    }

    m_downloadUrl = downloadUrl;
    m_releaseNotes = obj.value("body").toString();
    emit updateAvailable(latestVersion, m_downloadUrl, m_releaseNotes);
}

void UpdateManager::downloadUpdate(const QString& savePath) {
    if (m_downloadUrl.isEmpty()) {
        emit updateError(tr("No download URL available. Call checkForUpdates() first."));
        return;
    }

    QNetworkRequest request((QUrl(m_downloadUrl)));
    request.setHeader(QNetworkRequest::UserAgentHeader, "VoiceIME-Updater-Client");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    auto* file = new QSaveFile(savePath, this);
    if (!file->open(QIODevice::WriteOnly)) {
        emit updateError(tr("Could not open file for writing: %1").arg(file->errorString()));
        delete file;
        return;
    }

    QNetworkReply* reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::downloadProgress, this, &UpdateManager::downloadProgress);
    connect(reply, &QNetworkReply::readyRead, this, [reply, file]() {
        file->write(reply->readAll());
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, savePath, file]() {
        onDownloadFinished(reply, savePath, file);
    });
}

void UpdateManager::onDownloadFinished(QNetworkReply* reply, const QString& savePath, QSaveFile* file) {
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        file->cancelWriting();
        file->deleteLater();
        emit updateError(reply->errorString());
        return;
    }

    if (!file->commit()) {
        emit updateError(tr("Failed to save downloaded file: %1").arg(file->errorString()));
        file->deleteLater();
        return;
    }
    file->deleteLater();

    if (!QFile::exists(savePath)) {
        emit updateError(tr("Download finished but file was not found on disk."));
        return;
    }

    emit downloadFinished(savePath);
}

bool UpdateManager::launchUpdaterAndPrepareExit(const QString& downloadedFilePath, QString* errorMessage) {
    if (!QFile::exists(downloadedFilePath)) {
        if (errorMessage) *errorMessage = "Downloaded update file not found.";
        return false;
    }
#if defined(Q_OS_WIN)
    return launchUpdaterWindows(downloadedFilePath, errorMessage);
#elif defined(Q_OS_MAC)
    return launchUpdaterMacOS(downloadedFilePath, errorMessage);
#elif defined(Q_OS_LINUX)
    return launchUpdaterLinux(downloadedFilePath, errorMessage);
#else
    if (errorMessage) *errorMessage = "Auto-update not supported on this platform.";
    return false;
#endif
}

bool UpdateManager::launchUpdaterLinux(const QString& downloadedFilePath, QString* errorMessage) {
    const qint64 pid = QCoreApplication::applicationPid();
    const QString targetPath = QCoreApplication::applicationFilePath();

    QString script = QString(
        "#!/bin/bash\n"
        "while kill -0 %1 2>/dev/null; do sleep 0.5; done\n"
        "mv -f \"%2\" \"%3\"\n"
        "chmod +x \"%3\"\n"
        "\"%3\" &\n"
        "rm -- \"$0\"\n"
    ).arg(pid).arg(downloadedFilePath, targetPath);

    const QString scriptPath = QDir::tempPath() + "/voice_ime_updater.sh";
    QFile f(scriptPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMessage) *errorMessage = "Could not create updater script.";
        return false;
    }
    f.write(script.toUtf8());
    f.close();
    f.setPermissions(f.permissions() | QFileDevice::ExeOwner | QFileDevice::ExeGroup);

    return QProcess::startDetached("/bin/bash", { scriptPath });
}

bool UpdateManager::launchUpdaterMacOS(const QString& downloadedFilePath, QString* errorMessage) {
    const qint64 pid = QCoreApplication::applicationPid();
    QString targetApp = QDir::cleanPath(QCoreApplication::applicationDirPath() + "/../../..");

    QString script = QString(
        "#!/bin/bash\n"
        "while kill -0 %1 2>/dev/null; do sleep 0.5; done\n"
        "rm -rf \"%3\"\n"
        "cp -R \"%2\" \"%3\"\n"
        "open \"%3\"\n"
        "rm -- \"$0\"\n"
    ).arg(pid).arg(downloadedFilePath, targetApp);

    const QString scriptPath = QDir::tempPath() + "/voice_ime_updater.sh";
    QFile f(scriptPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMessage) *errorMessage = "Could not create updater script.";
        return false;
    }
    f.write(script.toUtf8());
    f.close();
    f.setPermissions(f.permissions() | QFileDevice::ExeOwner | QFileDevice::ExeGroup);

    return QProcess::startDetached("/bin/bash", { scriptPath });
}

bool UpdateManager::launchUpdaterWindows(const QString& downloadedFilePath, QString* errorMessage) {
    if (!QFile::exists(downloadedFilePath)) {
        if (errorMessage) *errorMessage = "Downloaded update file not found.";
        return false;
    }

    const QString currentExeName = QFileInfo(QCoreApplication::applicationFilePath()).fileName();
    const QString targetExePath = QFileInfo(QCoreApplication::applicationFilePath()).absoluteFilePath();

    // 逻辑：循环等待当前进程退出 -> 移动新文件覆盖旧文件 -> 重启程序 -> 校验并删除自身
    QString batchContent = QString(
        "@echo off\n"
        "setlocal enabledelayedexpansion\n"
        "chcp 65001 > nul\n"
        "echo [Updater] Waiting for application to exit...\n"
        ":loop\n"
        "tasklist /fi \"imagename eq %1\" | find /i \"%1\" > nul\n"
        "if %%errorlevel%% equ 0 (\n"
        "    timeout /t 1 > nul\n"
        "    goto loop\n"
        ")\n"
        "echo [Updater] Application closed. Running installer silently...\n"
        "\"%2\" /VERYSILENT /SUPPRESSMSGBOXES /NORESTART /NOCANCEL /SP-\n"
        "if  !errorlevel! neq 0 (\n"
        "    echo [Updater] Installer exited with code !errorlevel! (continuing update).\n"
        ")\n"
        "echo [Updater] Update done. Restarting...\n"
        "start \"\" \"%3\"\n"
        "del \"%2\" > nul 2>&1\n"
        "(goto) 2>nul & del \"%~f0\"\n"
    ).arg(currentExeName, downloadedFilePath, targetExePath);

    const QString batchPath = QDir::tempPath() + "/voice_ime_updater.bat";
    QFile batchFile(batchPath);
    if (!batchFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMessage) *errorMessage = "Could not create updater script.";
        return false;
    }   
    batchFile.write(batchContent.toUtf8());
    batchFile.close();

    qDebug() << "Launching updater script:" << batchPath;
    bool success = QProcess::startDetached("cmd.exe", { "/c", "start", "/min", "", QDir::toNativeSeparators(batchPath) });
    if (!success) {
        if (errorMessage) *errorMessage = "Failed to launch the updater script.";
        return false;
    }

    return true;
}