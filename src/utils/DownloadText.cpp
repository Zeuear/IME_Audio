#include "DownloadText.h"
#include <QStringList>
#include <algorithm>

namespace DownloadText {

QString formatSize(qint64 bytes)
{
    if (bytes < 0) return QStringLiteral("?");
    constexpr double kKB = 1024.0, kMB = kKB * 1024.0, kGB = kMB * 1024.0;
    if (bytes < 1024) return QStringLiteral("%1 B").arg(bytes);
    if (bytes < kMB) return QStringLiteral("%1 KB").arg(bytes / kKB, 0, 'f', 1);
    if (bytes < kGB) return QStringLiteral("%1 MB").arg(bytes / kMB, 0, 'f', 1);
    return QStringLiteral("%1 GB").arg(bytes / kGB, 0, 'f', 2);
}

namespace {
QString elideMiddle(const QString& name, int maxChars)
{
    if (maxChars < 4 || name.size() <= maxChars) return name;
    const int keep = maxChars - 1;        // 留一个位置给省略号
    const int head = keep / 2;
    const int tail = keep - head;         // 尾部多留一点，扩展名和版本号都在尾巴上
    return name.left(head) + QChar(0x2026) + name.right(tail);
}
}

QString progressLine(const QString& fileName, int fileIndex, int fileCount, int percent, int maxNameChars)
{
    QStringList parts;
    if (!fileName.isEmpty()) parts << elideMiddle(fileName, maxNameChars);
    if (fileCount > 1) parts << QStringLiteral("%1/%2").arg(std::clamp(fileIndex, 1, fileCount)).arg(fileCount);
    if (percent >= 0) parts << QStringLiteral("%1%").arg(std::min(percent, 100));
    return parts.join(QStringLiteral(" · "));
}

} // namespace DownloadText

void DownloadGroupProgress::start(int totalFiles)
{
    m_total = qMax(0, totalFiles);
    m_finished = 0;
    m_percent = 0;
    m_name.clear();
    m_hasUpdate = false;
}

void DownloadGroupProgress::fileFinished()
{
    m_finished = qMin(m_finished + 1, m_total);
}

void DownloadGroupProgress::update(const QString& fileName, int overallPercent)
{
    m_name = fileName;
    m_percent = std::clamp(overallPercent, 0, 100);
    m_hasUpdate = true;
}

QString DownloadGroupProgress::line(int maxNameChars) const
{
    if (!m_hasUpdate) return {};
    return DownloadText::progressLine(m_name, m_finished + 1, m_total, m_percent, maxNameChars);
}
