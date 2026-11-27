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

QString dictionariesDir()
{
    return QDir(dataDir()).absoluteFilePath("dictionaries");
}

QString snippetsFile()
{
    return QDir(dataDir()).absoluteFilePath("snippets/data/snippets.json");
}

QString promptsDir()
{
    return QDir(dataDir()).absoluteFilePath("prompts");
}

QString transcriptFile()
{
    return QDir(dataDir()).absoluteFilePath("transcripts/live_transcript.txt");
}

QString toolsDir()
{
    return QDir(dataDir()).absoluteFilePath("tools");
}

QString linkDownloadsDir()
{
    return QDir(dataDir()).absoluteFilePath("link_downloads");
}

QString separatedDir()
{
    return QDir(dataDir()).absoluteFilePath("separated");
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

QString translationModelsDir()
{
    const QString dir = QDir(dataDir()).absoluteFilePath("translation/models");
    if (!QFile::exists(dir)) {
        QDir().mkpath(dir);
    }
    return dir;
}

QString vadModelFile()
{
    const QString bundled = QDir(resourceDir()).absoluteFilePath("sherpa/vad/silero_vad.onnx");
#ifdef Q_OS_MACOS
    // VAD 模型构建时拷进 bundle Resources，缺失时下载到数据目录（bundle 内不可写），两处都要认。
    if (!QFile::exists(bundled)) {
        return QDir(sherpaRoot()).absoluteFilePath("vad/silero_vad.onnx");
    }
#endif
    return bundled;
}

}
