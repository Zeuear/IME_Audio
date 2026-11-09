#include "TranscriptStore.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include "config/ConfigManager.h"

namespace {
bool writeAtomically(const QString& path, const QString& text)
{
    // 只给当前用户读写
    return ConfigManager::writeFileAtomically(path, text.toUtf8(), true);
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
