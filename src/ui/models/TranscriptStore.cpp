#include "TranscriptStore.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

namespace {
bool writeAtomically(const QString& path, const QString& text)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return false;
    // 只给当前用户读写；Windows 上主要靠程序目录本身的权限，这一步效果有限
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    if (file.write(text.toUtf8()) < 0) {
        file.cancelWriting();
        return false;
    }
    return file.commit();
}
} // namespace

TranscriptStore::TranscriptStore(const QString& sessionFile)
    : m_path(sessionFile)
{
}

bool TranscriptStore::save(const QString& text)
{
    if (!QDir().mkpath(QFileInfo(m_path).absolutePath())) return false;
    return writeAtomically(m_path, text);
}

QString TranscriptStore::load() const
{
    QFile file(m_path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    return QString::fromUtf8(file.readAll());
}

void TranscriptStore::remove()
{
    QFile::remove(m_path);
}

bool TranscriptStore::exportTxt(const QString& path, const QString& text)
{
    return writeAtomically(path, text);
}
