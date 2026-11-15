#include "TranscriptLineModel.h"
#include <QRegularExpression>
#include <QStringList>

namespace {
const QString kTimeFormat = QStringLiteral("hh:mm:ss");
}

TranscriptLineModel::TranscriptLineModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int TranscriptLineModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_entries.size();
}

QVariant TranscriptLineModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= m_entries.size()) return {};
    const Entry& e = m_entries.at(index.row());
    if (role == Qt::DisplayRole) return e.text;
    if (role == TimeRole) return e.time.isValid() ? e.time.toString(kTimeFormat) : QString();
    return {};
}

void TranscriptLineModel::add(const QString& text, const QTime& time)
{
    static const QRegularExpression kLineBreaks(QStringLiteral("\\s*[\\r\\n\\x{2029}]+\\s*"));
    QString t = text.trimmed();
    t.replace(kLineBreaks, QStringLiteral(" "));
    if (t.isEmpty()) return;
    const int row = m_entries.size();
    beginInsertRows({}, row, row);
    m_entries.append({ time, t });
    endInsertRows();
}

void TranscriptLineModel::clear()
{
    if (m_entries.isEmpty()) return;
    beginResetModel();
    m_entries.clear();
    endResetModel();
}

QString TranscriptLineModel::textAt(int row) const
{
    return row >= 0 && row < m_entries.size() ? m_entries.at(row).text : QString();
}

QString TranscriptLineModel::lineOf(const Entry& e, bool withTime)
{
    if (!withTime || !e.time.isValid()) return e.text;
    return QStringLiteral("[%1] %2").arg(e.time.toString(kTimeFormat), e.text);
}

QString TranscriptLineModel::linesText(const QList<int>& rows, bool withTime) const
{
    QStringList lines;
    lines.reserve(rows.size());
    for (int row : rows) {
        if (row >= 0 && row < m_entries.size()) lines << lineOf(m_entries.at(row), withTime);
    }
    return lines.join(QLatin1Char('\n'));
}

QString TranscriptLineModel::allText(bool withTime) const
{
    QStringList lines;
    lines.reserve(m_entries.size());
    for (const Entry& e : m_entries) lines << lineOf(e, withTime);
    return lines.join(QLatin1Char('\n'));
}

void TranscriptLineModel::restore(const QString& serialized)
{
    static const QRegularExpression kPrefixed(QStringLiteral("^\\[(\\d{2}:\\d{2}:\\d{2})\\] (.*)$"));
    QList<Entry> entries;
    const QStringList lines = serialized.split(QLatin1Char('\n'));
    for (const QString& raw : lines) {
        const QString line = raw.trimmed();
        if (line.isEmpty()) continue;
        const QRegularExpressionMatch m = kPrefixed.match(line);
        const QTime time = m.hasMatch() ? QTime::fromString(m.captured(1), kTimeFormat) : QTime();
        const QString text = time.isValid() ? m.captured(2).trimmed() : line;
        if (!text.isEmpty()) entries.append({ time, text });
    }
    beginResetModel();
    m_entries = std::move(entries);
    endResetModel();
}
