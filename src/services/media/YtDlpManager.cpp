#include "YtDlpManager.h"
#include "MediaLink.h"
#include "utils/AppPaths.h"
#include "utils/Downloader.h"
#include "utils/Logger.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QSysInfo>
#include <QTimer>
#include <QUuid>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {

constexpr int kSumsTimeoutMs = 15000;
constexpr int kToolStallMs = 30000;         // 下载组件时无数据多久放弃
constexpr int kJobStallMs = 90000;          // 下载音频时无输出多久终止
constexpr qint64 kCheckIntervalSec = 3600;
const char* kReleaseBase = "https://github.com/yt-dlp/yt-dlp/releases/latest/download/";

struct ToolSpec {
    bool supported = false;
    QString assetName;   // Release 里的文件名，也是 SHA2-256SUMS 里的键
    QString localName;
};

ToolSpec toolSpec()
{
    ToolSpec s;
#if defined(Q_OS_WIN)
    s = { true, QStringLiteral("yt-dlp.exe"), QStringLiteral("yt-dlp.exe") };
#elif defined(Q_OS_MACOS)
    s = { true, QStringLiteral("yt-dlp_macos"), QStringLiteral("yt-dlp") };
#elif defined(Q_OS_LINUX)
    const bool arm = QSysInfo::currentCpuArchitecture() == QLatin1String("arm64");
    s = { true, arm ? QStringLiteral("yt-dlp_linux_aarch64") : QStringLiteral("yt-dlp_linux"), QStringLiteral("yt-dlp") };
#endif
    return s;
}

QString sha256OfFile(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&f)) return {};
    return QString::fromLatin1(hash.result().toHex());
}

QString toolDir() { return QDir(AppPaths::toolsDir()).absoluteFilePath(QStringLiteral("yt-dlp")); }
QString stampPath() { return QDir(toolDir()).absoluteFilePath(QStringLiteral("last_check")); }

void removeDir(const QString& dir)
{
    if (!dir.isEmpty()) QDir(dir).removeRecursively();
}

}

YtDlpManager::YtDlpManager(QNetworkAccessManager* nam, QObject* parent)
    : QObject(parent), m_nam(nam)
{
    m_watchdog = new QTimer(this);
    m_watchdog->setSingleShot(true);
    m_watchdog->setInterval(kJobStallMs);
    connect(m_watchdog, &QTimer::timeout, this, [this]() {
        if (!m_process) return;
        LOG_WARN("YtDlpManager: download stalled, terminating yt-dlp");
        m_timedOut = true;
        m_process->kill();
    });

    // 上次运行残留的下载（异常退出）没有任何任务还在引用，直接清掉
    removeDir(AppPaths::linkDownloadsDir());
}

YtDlpManager::~YtDlpManager()
{
    if (m_process) {
        m_cancelled = true;
        m_process->kill();
        m_process->waitForFinished(2000);
    }
    removeDir(AppPaths::linkDownloadsDir());
}

bool YtDlpManager::isPlatformSupported()
{
    return toolSpec().supported;
}

quint64 YtDlpManager::enqueue(const QString& url)
{
    Job job;
    job.id = m_nextId++;
    job.url = url;
    m_queue.append(job);
    QTimer::singleShot(0, this, &YtDlpManager::pump);
    return job.id;
}

void YtDlpManager::cancel(quint64 id)
{
    if (m_phase == Phase::Running && m_current.id == id) {
        m_cancelled = true;
        if (m_process) m_process->kill();
        return;
    }
    for (int i = 0; i < m_queue.size(); ++i) {
        if (m_queue.at(i).id == id) { m_queue.removeAt(i); break; }
    }
    release(id);
}

void YtDlpManager::cancelAll()
{
    m_queue.clear();
    if (m_phase == Phase::Running) {
        m_cancelled = true;
        if (m_process) m_process->kill();
    }
    const auto dirs = m_workDirs;
    for (auto it = dirs.begin(); it != dirs.end(); ++it) removeDir(it.value());
    m_workDirs.clear();
}

void YtDlpManager::release(quint64 id)
{
    const auto it = m_workDirs.find(id);
    if (it == m_workDirs.end()) return;
    removeDir(it.value());
    m_workDirs.erase(it);
}

// ---------------------------------------------------------------- 下载组件
QString YtDlpManager::toolPath() const
{
    return QDir(toolDir()).absoluteFilePath(toolSpec().localName);
}

bool YtDlpManager::toolIsFresh() const
{
    if (!QFile::exists(toolPath())) return false;
    const QFileInfo stamp(stampPath());
    if (!stamp.exists()) return false;
    return stamp.lastModified().secsTo(QDateTime::currentDateTime()) < kCheckIntervalSec;
}

void YtDlpManager::touchCheckStamp() const
{
    QDir().mkpath(toolDir());
    QFile f(stampPath());
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) f.write(QDateTime::currentDateTimeUtc().toString(Qt::ISODate).toUtf8());
}

void YtDlpManager::pump()
{
    if (m_phase != Phase::Idle || m_queue.isEmpty()) return;

    if (!isPlatformSupported()) {
        const QList<Job> jobs = m_queue;
        m_queue.clear();
        for (const Job& j : jobs)
            emit taskFinished(j.id, false, {}, {}, QStringLiteral("当前系统暂不支持链接下载"));
        return;
    }

    const bool force = m_forceUpdate;
    m_forceUpdate = false;
    prepareTool(force);
}

void YtDlpManager::prepareTool(bool force)
{
    m_phase = Phase::Preparing;
    if (!force && toolIsFresh()) {
        toolReady();
        return;
    }
    emit toolStatus(tr("Checking downloader update..."));
    fetchSums(QFile::exists(toolPath()));
}

QNetworkReply* YtDlpManager::get(const QString& url, int timeoutMs)
{
    QNetworkRequest req{QUrl(url)};
    req.setRawHeader("User-Agent", "ImeAudio");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    QNetworkReply* reply = m_nam->get(req);
    // 计时器挂在 reply 上：请求结束或销毁时一起消失；超时就中止，由 finished 统一收尾
    auto* timer = new QTimer(reply);
    timer->setObjectName(QStringLiteral("stall"));
    timer->setSingleShot(true);
    timer->start(timeoutMs);
    connect(timer, &QTimer::timeout, reply, &QNetworkReply::abort);
    return reply;
}

void YtDlpManager::fetchSums(bool toolExists)
{
    const ToolSpec spec = toolSpec();
    QNetworkReply* reply = get(QString::fromLatin1(kReleaseBase) + QStringLiteral("SHA2-256SUMS"), kSumsTimeoutMs);
    connect(reply, &QNetworkReply::finished, this, [this, reply, toolExists, spec]() {
        reply->deleteLater();
        const QString expected = reply->error() == QNetworkReply::NoError
                                     ? MediaLink::sha256For(reply->readAll(), spec.assetName)
                                     : QString();
        if (expected.isEmpty()) {
            LOG_WARN(QString("YtDlpManager: cannot get yt-dlp checksums: %1").arg(reply->errorString()));
            if (toolExists) {   // 查不到更新不该耽误下载：用现有的
                toolReady();
            } else {
                failPreparation(QStringLiteral("无法获取下载组件，请检查网络后重试"));
            }
            return;
        }
        if (toolExists && sha256OfFile(toolPath()) == expected) {
            touchCheckStamp();
            toolReady();
            return;
        }
        downloadTool(expected);
    });
}

void YtDlpManager::downloadTool(const QString& expectedSha)
{
    const ToolSpec spec = toolSpec();
    const QString checksumError = QStringLiteral("下载组件校验失败，已放弃使用");

    emit toolStatus(tr("Downloading media component... %1%").arg(0));
    auto* dl = new Downloader(m_nam, this);
    m_toolDownloader = dl;
    dl->setUserAgent("ImeAudio");
    dl->setStallTimeoutMs(kToolStallMs);
    dl->setVerifier([expectedSha, checksumError](const QString& partPath) {
        return sha256OfFile(partPath) == expectedSha ? QString() : checksumError;
    });

    connect(dl, &Downloader::progress, this, [this](const QString&, qint64 received, qint64 total, double) {
        if (total > 0) emit toolStatus(tr("Downloading media component... %1%").arg(static_cast<int>(received * 100 / total)));
    });
    connect(dl, &Downloader::finished, this, [this, dl]() {
        dl->deleteLater();
        if (m_toolDownloader == dl) m_toolDownloader = nullptr;
#ifndef Q_OS_WIN
        QFile::setPermissions(toolPath(), QFile::permissions(toolPath()) | QFileDevice::ExeOwner | QFileDevice::ExeGroup | QFileDevice::ExeOther);
#endif
        touchCheckStamp();
        LOG_INFO("YtDlpManager: yt-dlp updated");
        toolReady();
    });
    connect(dl, &Downloader::error, this, [this, dl, checksumError](const QString& err) {
        dl->deleteLater();
        if (m_toolDownloader == dl) m_toolDownloader = nullptr;
        LOG_WARN(QString("YtDlpManager: download of yt-dlp failed: %1").arg(err));
        if (QFile::exists(toolPath())) {   // 更新失败不该耽误下载：用现有的
            toolReady();
            return;
        }
        const bool diskError = err.startsWith(QStringLiteral("无法创建文件")) || err.startsWith(QStringLiteral("写入文件失败"))
                               || err.startsWith(QStringLiteral("重命名文件失败"));
        failPreparation(err == checksumError ? checksumError
                        : diskError          ? QStringLiteral("无法写入下载组件，请检查磁盘空间和目录权限")
                                             : QStringLiteral("下载组件下载失败，请检查网络后重试"));
    });
    dl->start(QUrl(QString::fromLatin1(kReleaseBase) + spec.assetName), toolPath());
}

void YtDlpManager::toolReady()
{
    emit toolStatus(QString());
    m_phase = Phase::Idle;
    if (m_queue.isEmpty()) return;
    startJob();
}

void YtDlpManager::failPreparation(const QString& error)
{
    emit toolStatus(QString());
    m_phase = Phase::Idle;
    // 组件取不到时，排队中的任务都不可能成功：一并失败，而不是一个个重复尝试联网
    const QList<Job> jobs = m_queue;
    m_queue.clear();
    for (const Job& j : jobs) emit taskFinished(j.id, false, {}, {}, error);
}

// ---------------------------------------------------------------- 下载音频
void YtDlpManager::startJob()
{
    m_current = m_queue.takeFirst();
    m_phase = Phase::Running;
    m_cancelled = false;
    m_timedOut = false;
    m_stderr.clear();

    m_workDir = QDir(AppPaths::linkDownloadsDir())
                    .absoluteFilePath(QStringLiteral("%1_%2").arg(m_current.id).arg(QUuid::createUuid().toString(QUuid::Id128).left(8)));
    QDir().mkpath(m_workDir);
    const QString resultFile = QDir(m_workDir).absoluteFilePath(QStringLiteral("result.txt"));

    m_process = new QProcess(this);
    m_process->setProgram(toolPath());
    m_process->setArguments({
        QStringLiteral("--ignore-config"),          // 不读用户的配置文件，避免被塞进别的参数
        QStringLiteral("--no-playlist"),
        QStringLiteral("--playlist-items"), QStringLiteral("1"),   // 一条链接只取第一个视频，结果一一对应
        QStringLiteral("--no-warnings"),
        QStringLiteral("--newline"),
        QStringLiteral("--progress"),
        QStringLiteral("--no-cache-dir"),
        QStringLiteral("--no-part"),
        QStringLiteral("--no-mtime"),
        QStringLiteral("--socket-timeout"), QStringLiteral("30"),
        QStringLiteral("--retries"), QStringLiteral("3"),
        QStringLiteral("--max-filesize"), QStringLiteral("2G"),
        // 只要音频；没有单独音轨时取带音频的整体，解码交给文件转录原有的解码器，不依赖 ffmpeg 合并
        QStringLiteral("-f"), QStringLiteral("bestaudio/best"),
        QStringLiteral("-o"), QDir(m_workDir).absoluteFilePath(QStringLiteral("%(id)s.%(ext)s")),
        QStringLiteral("--print"), QStringLiteral("before_dl:TITLE:%(title)s"),
        // 结果写进 UTF-8 文件，比解析标准输出可靠（标准输出的编码取决于系统）
        QStringLiteral("--print-to-file"), QStringLiteral("after_move:%(title)s\t%(filepath)s"), resultFile,
        QStringLiteral("--"),                       // 此后全是位置参数，链接不会被当成选项
        m_current.url,
    });
    m_process->setWorkingDirectory(m_workDir);

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("PYTHONIOENCODING"), QStringLiteral("utf-8"));
    env.insert(QStringLiteral("PYTHONUTF8"), QStringLiteral("1"));
    m_process->setProcessEnvironment(env);
#ifdef Q_OS_WIN
    m_process->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* args) {
        args->flags |= CREATE_NO_WINDOW;
    });
#endif

    const quint64 id = m_current.id;
    connect(m_process, &QProcess::readyReadStandardOutput, this, [this, id]() {
        m_watchdog->start();
        const QString out = QString::fromUtf8(m_process->readAllStandardOutput());
        for (const QString& raw : out.split(QRegularExpression(QStringLiteral("[\\r\\n]+")), Qt::SkipEmptyParts)) {
            const QString line = raw.trimmed();
            if (line.startsWith(QLatin1String("TITLE:"))) {
                emit taskTitle(id, line.mid(6).trimmed());
                continue;
            }
            const int pct = MediaLink::parseProgressPercent(line);
            if (pct >= 0) emit taskProgress(id, pct);
        }
    });
    connect(m_process, &QProcess::readyReadStandardError, this, [this]() {
        m_watchdog->start();
        m_stderr += QString::fromUtf8(m_process->readAllStandardError());
        if (m_stderr.size() > 8192) m_stderr.remove(0, m_stderr.size() - 8192);   // 只留末尾，够判断原因
    });
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError err) {
        if (err != QProcess::FailedToStart) return;
        LOG_ERROR("YtDlpManager: yt-dlp failed to start");
        failCurrent(QStringLiteral("无法启动下载组件"));
    });
    connect(m_process, &QProcess::finished, this, [this](int exitCode, QProcess::ExitStatus) { onProcessFinished(exitCode); });

    LOG_INFO(QString("YtDlpManager: downloading audio for task %1").arg(id));
    emit taskStarted(id);
    m_watchdog->start();
    m_process->start();
}

void YtDlpManager::onProcessFinished(int exitCode)
{
    m_watchdog->stop();
    if (!m_process) return;   // failCurrent 已经收尾

    if (m_cancelled) {
        removeDir(m_workDir);
        cleanupCurrent();
        QTimer::singleShot(0, this, &YtDlpManager::pump);
        return;
    }
    if (m_timedOut) {
        failCurrent(QStringLiteral("下载超时，请稍后重试"));
        return;
    }

    QString title, path;
    QFile result(QDir(m_workDir).absoluteFilePath(QStringLiteral("result.txt")));
    if (exitCode == 0 && result.open(QIODevice::ReadOnly)) {
        const QStringList lines = QString::fromUtf8(result.readAll()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        if (!lines.isEmpty()) MediaLink::parseResultLine(lines.first().trimmed(), &title, &path);
    }
    if (!path.isEmpty() && QFileInfo::exists(path)) {
        finishCurrentSuccess(title, path);
        return;
    }

    const MediaLink::Failure failure = MediaLink::classifyFailure(m_stderr);
    LOG_WARN(QString("YtDlpManager: yt-dlp failed (exit %1): %2").arg(exitCode).arg(m_stderr.right(600)));

    // 站点改版导致解析失败：先强制更新组件，再重试这一个任务一次
    if (failure == MediaLink::Failure::ExtractorBroken && !m_current.retried) {
        Job retry = m_current;
        retry.retried = true;
        removeDir(m_workDir);
        cleanupCurrent();
        m_queue.prepend(retry);
        m_forceUpdate = true;
        QTimer::singleShot(0, this, &YtDlpManager::pump);
        return;
    }
    failCurrent(MediaLink::failureMessage(failure));
}

void YtDlpManager::finishCurrentSuccess(const QString& title, const QString& path)
{
    const quint64 id = m_current.id;
    m_workDirs.insert(id, m_workDir);   // 文件还要被转录读取，等 release 再删
    cleanupCurrent();
    emit taskFinished(id, true, path, title, {});
    QTimer::singleShot(0, this, &YtDlpManager::pump);
}

void YtDlpManager::failCurrent(const QString& error)
{
    const quint64 id = m_current.id;
    if (m_process) m_process->kill();
    removeDir(m_workDir);
    cleanupCurrent();
    emit taskFinished(id, false, {}, {}, error);
    QTimer::singleShot(0, this, &YtDlpManager::pump);
}

void YtDlpManager::cleanupCurrent()
{
    m_watchdog->stop();
    if (m_process) {
        m_process->disconnect(this);
        m_process->deleteLater();
        m_process = nullptr;
    }
    m_workDir.clear();
    m_phase = Phase::Idle;
}
