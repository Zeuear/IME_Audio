#include "Diagnostics.h"

#include <QAudioDevice>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QLocale>
#include <QMediaDevices>
#include <QMutex>
#include <QMutexLocker>
#include <QSysInfo>
#include <QTextStream>

#include <cstdio>

#ifdef Q_OS_MACOS
#include <sys/sysctl.h>
#include <sys/xattr.h>
#include <unistd.h>
#endif

#include "AppPaths.h"
#include "Logger.h"
#include "PlatformPermissions.h"
#include "version.h"

namespace {

QtMessageHandler s_previousHandler = nullptr;
QMutex s_handlerMutex;

const char* levelName(QtMsgType type)
{
    switch (type) {
    case QtWarningMsg:  return "QT-WARN";
    case QtCriticalMsg: return "QT-ERROR";
    case QtFatalMsg:    return "QT-FATAL";
    default:            return "QT-INFO";
    }
}

// 不能经由 Logger 写：Logger::log 持锁期间自己也会调用 qWarning，走回这里会死锁。
void messageHandler(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
    if (type == QtWarningMsg || type == QtCriticalMsg || type == QtFatalMsg) {
        QMutexLocker locker(&s_handlerMutex);
        QFile file(AppPaths::logFile());
        if (file.open(QIODevice::Append | QIODevice::Text)) {
            QTextStream out(&file);
            out.setEncoding(QStringConverter::Utf8);
            out << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz")
                << " | " << levelName(type) << " | " << message;
            if (context.category && qstrcmp(context.category, "default") != 0) {
                out << " [" << context.category << "]";
            }
            out << "\n";
        }
    }

    if (s_previousHandler) {
        s_previousHandler(type, context, message);
    } else {
        // qInstallMessageHandler 在此前用的是 Qt 默认处理器时返回 nullptr，
        // 这里不补的话开发时控制台就再也看不到 Qt 的输出了。
        fprintf(stderr, "%s\n", qPrintable(qFormatLogMessage(type, context, message)));
        fflush(stderr);
    }
}

QLatin1String statusName(PlatformPermissions::Status status)
{
    switch (status) {
    case PlatformPermissions::Status::NotRequired:   return QLatin1String("not-required");
    case PlatformPermissions::Status::Granted:       return QLatin1String("granted");
    case PlatformPermissions::Status::Denied:        return QLatin1String("denied");
    case PlatformPermissions::Status::NotDetermined: return QLatin1String("not-determined");
    }
    return QLatin1String("unknown");
}

void logAudioDevices()
{
    const QByteArray defaultInputId = QMediaDevices::defaultAudioInput().id();
    const QByteArray defaultOutputId = QMediaDevices::defaultAudioOutput().id();

    const auto inputs = QMediaDevices::audioInputs();
    LOG_DEBUG(QString("Env | audio inputs: %1").arg(inputs.size()));
    for (const QAudioDevice& d : inputs) {
        LOG_DEBUG(QString("Env |   in  %1 %2 (id=%3, rate=%4-%5, ch=%6-%7)")
                      .arg(d.id() == defaultInputId ? QLatin1String("*") : QLatin1String("-"), d.description(), QString::fromUtf8(d.id()))
                      .arg(d.minimumSampleRate()).arg(d.maximumSampleRate())
                      .arg(d.minimumChannelCount()).arg(d.maximumChannelCount()));
    }

    const auto outputs = QMediaDevices::audioOutputs();
    LOG_DEBUG(QString("Env | audio outputs: %1").arg(outputs.size()));
    for (const QAudioDevice& d : outputs) {
        LOG_DEBUG(QString("Env |   out %1 %2 (id=%3)")
                      .arg(d.id() == defaultOutputId ? QLatin1String("*") : QLatin1String("-"), d.description(), QString::fromUtf8(d.id())));
    }
}

#ifdef Q_OS_MACOS
void logMacLaunchContext()
{
    int translated = 0;
    size_t size = sizeof(translated);
    if (sysctlbyname("sysctl.proc_translated", &translated, &size, nullptr, 0) != 0) {
        translated = 0;
    }
    // 编译架构与运行架构不一致（Apple Silicon 上跑 x86_64 包）时，arm64 的 sherpa 动态库
    // 会加载失败，反之亦然。
    LOG_DEBUG(QString("Env | rosetta translated: %1 (build arch=%2, cpu arch=%3)")
                  .arg(translated ? QLatin1String("yes") : QLatin1String("no"), QSysInfo::buildCpuArchitecture(), QSysInfo::currentCpuArchitecture()));

    const QString appDir = QCoreApplication::applicationDirPath();
    const QString bundle = QDir::cleanPath(appDir + "/../..");
    LOG_DEBUG(QString("Env | bundle: %1").arg(bundle));

    // Gatekeeper 的 App Translocation：未移出 DMG / 下载目录的应用会被挂载到只读随机路径，
    // 相对 bundle 的资源查找和数据目录行为都会变。
    if (appDir.contains("/AppTranslocation/")) {
        LOG_WARN("Env | 应用运行在 Gatekeeper 的 AppTranslocation 沙盒路径下，请先把 .app 拖进「应用程序」再启动");
    }
    if (appDir.startsWith("/Volumes/")) {
        LOG_WARN("Env | 应用正从挂载卷(DMG)直接运行，请先拷贝到「应用程序」");
    }

    const QByteArray bundleUtf8 = QFile::encodeName(bundle);
    if (getxattr(bundleUtf8.constData(), "com.apple.quarantine", nullptr, 0, 0, XATTR_NOFOLLOW) >= 0) {
        LOG_WARN("Env | bundle 带有 com.apple.quarantine 隔离标记（可执行 xattr -dr com.apple.quarantine <app> 清除）");
    }
}

void redirectStderr()
{
    // 从终端启动时保持原样，方便开发者直接看输出。
    if (isatty(fileno(stderr))) return;

    const QByteArray path = QFile::encodeName(QDir(AppPaths::dataDir()).absoluteFilePath("voice_ime.stderr.log"));
    if (!freopen(path.constData(), "a", stderr)) return;
    setvbuf(stderr, nullptr, _IOLBF, 0);
    fprintf(stderr, "==== stderr redirected %s pid=%lld ====\n",
            QDateTime::currentDateTime().toString(Qt::ISODateWithMs).toLocal8Bit().constData(),
            QCoreApplication::applicationPid());
}
#endif

} // namespace

namespace Diagnostics {

void install()
{
    s_previousHandler = qInstallMessageHandler(messageHandler);
#ifdef Q_OS_MACOS
    redirectStderr();
#endif
}

void logEnvironment()
{
    LOG_DEBUG("==================== session start ====================");
    LOG_DEBUG(QString("Env | app version: %1, pid=%2").arg(PROJECT_VERSION).arg(QCoreApplication::applicationPid()));
    LOG_DEBUG(QString("Env | os: %1 (kernel %2 %3), arch=%4")
                  .arg(QSysInfo::prettyProductName(), QSysInfo::kernelType(), QSysInfo::kernelVersion(),
                       QSysInfo::currentCpuArchitecture()));
    LOG_DEBUG(QString("Env | Qt runtime %1 (built with %2)").arg(qVersion(), QT_VERSION_STR));
    LOG_DEBUG(QString("Env | executable: %1").arg(QCoreApplication::applicationFilePath()));
    LOG_DEBUG(QString("Env | locale: %1").arg(QLocale::system().name()));

#ifdef Q_OS_MACOS
    logMacLaunchContext();
#endif

    logFileState("Env | resources dir", AppPaths::resourceDir());
    logFileState("Env | vad model", AppPaths::vadModelFile());

    LOG_DEBUG(QString("Env | permission microphone=%1 accessibility=%2")
                  .arg(statusName(PlatformPermissions::microphoneStatus()),
                       statusName(PlatformPermissions::accessibilityStatus())));

    logAudioDevices();
}

void logFileState(const QString& label, const QString& path)
{
    const QFileInfo info(path);
    if (!info.exists()) {
        LOG_WARN(QString("%1: MISSING %2").arg(label, path));
        return;
    }
    if (info.isDir()) {
        LOG_DEBUG(QString("%1: dir ok %2 (readable=%3)").arg(label, path).arg(info.isReadable()));
        return;
    }
    LOG_DEBUG(QString("%1: %2 (%3 bytes, readable=%4)").arg(label, path).arg(info.size()).arg(info.isReadable()));
}

void logDirectoryListing(const QString& label, const QString& dir, int maxEntries)
{
    if (!QFileInfo(dir).isDir()) {
        LOG_WARN(QString("%1: directory missing %2").arg(label, dir));
        return;
    }

    const QDir root(dir);
    int count = 0;
    QDirIterator it(dir, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        if (++count > maxEntries) continue;
        LOG_DEBUG(QString("%1:   %2 (%3 bytes)").arg(label, root.relativeFilePath(it.filePath())).arg(it.fileInfo().size()));
    }

    if (count == 0) {
        LOG_WARN(QString("%1: directory is EMPTY %2（解压可能中断）").arg(label, dir));
    } else if (count > maxEntries) {
        LOG_DEBUG(QString("%1:   ... and %2 more files").arg(label).arg(count - maxEntries));
    }
}

}
