#include "InstallMarker.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace InstallMarker {

namespace {
QString markerPath(const QString& modelDir)
{
    return QDir(modelDir).absoluteFilePath(QStringLiteral(".installing"));
}
}

bool isInstalled(const QString& modelDir)
{
    return QFileInfo(modelDir).isDir() && !QFile::exists(markerPath(modelDir));
}

bool isIncomplete(const QString& modelDir)
{
    return QFileInfo(modelDir).isDir() && QFile::exists(markerPath(modelDir));
}

bool markInstalling(const QString& modelDir)
{
    if (!QDir().mkpath(modelDir)) return false;
    QFile marker(markerPath(modelDir));
    if (marker.exists()) return true;
    return marker.open(QIODevice::WriteOnly);
}

void markComplete(const QString& modelDir)
{
    QFile::remove(markerPath(modelDir));
}

} // namespace InstallMarker
