#include "AppPaths.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QStandardPaths>

namespace AppPaths {

QString dataDir()
{
#ifdef Q_OS_MACOS
    static const QString dir = [] {
        const QString d = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir().mkpath(d);
        return d;
    }();
    return dir;
#else
    return QCoreApplication::applicationDirPath();
#endif
}

QString resourceDir()
{
#ifdef Q_OS_MACOS
    return QDir::cleanPath(QCoreApplication::applicationDirPath() + "/../Resources");
#else
    return QCoreApplication::applicationDirPath();
#endif
}

QString logFile()
{
    return QDir(dataDir()).absoluteFilePath("voice_ime.log");
}

QString configFile()
{
    return QDir(dataDir()).absoluteFilePath("voice_ime.ini");
}

QString termsFile()
{
    return QDir(dataDir()).absoluteFilePath("terms.tsv");
}

QString promptsDir()
{
    // 提示词首次运行时会写入默认内容，属可写数据
    return QDir(dataDir()).absoluteFilePath("prompts");
}

QString sherpaRoot()
{
    return QDir(dataDir()).absoluteFilePath("sherpa");
}

QString sherpaModelsDir()
{
    const QString dir = QDir(sherpaRoot()).absoluteFilePath("models");
    if (!QFile::exists(dir)) {
        QDir().mkpath(dir);
    }
    return dir;
}

QString vadModelFile()
{
    const QString bundled = QDir(resourceDir()).absoluteFilePath("sherpa/vad/silero_vad.onnx");
#ifdef Q_OS_MACOS
    // 构建时会把 VAD 模型拷进 bundle 的 Resources；但 SherpaInstaller::ensureVadModel 缺失时
    // 下载到的是数据目录（bundle 内不可写）。两处都要认，否则下载成功了也永远找不到。
    if (!QFile::exists(bundled)) {
        return QDir(sherpaRoot()).absoluteFilePath("vad/silero_vad.onnx");
    }
#endif
    return bundled;
}

}
