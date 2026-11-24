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

namespace {
const QString kTranslationMark = QStringLiteral(">> ");
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
    if (role == TranslationRole) return e.translation;
    return {};
}

QString TranscriptLineModel::singleLine(const QString& text)
{
    static const QRegularExpression kLineBreaks(QStringLiteral("\\s*[\\r\\n\\x{2029}]+\\s*"));
    QString t = text.trimmed();
    t.replace(kLineBreaks, QStringLiteral(" "));
    return t;
}

void TranscriptLineModel::add(const QString& text, const QTime& time, const QString& translation)
{
    const QString t = singleLine(text);
    if (t.isEmpty()) return;
    const int row = m_entries.size();
    beginInsertRows({}, row, row);
    m_entries.append({ time, t, singleLine(translation) });
    endInsertRows();
}

void TranscriptLineModel::addPending(quint64 key, const QString& text, const QTime& time)
{
    const QString t = singleLine(text);
    if (t.isEmpty()) return;
    // 序号小的在前；从后往前找到第一个序号不大于它的等待行，插在后面（没有等待行就追加到末尾）
    int row = m_entries.size();
    while (row > 0 && m_entries.at(row - 1).key > key) --row;
    beginInsertRows({}, row, row);
    Entry e;
    e.time = time;
    e.text = t;
    e.key = key;
    e.pendingText = t;
    m_entries.insert(row, e);
    endInsertRows();
}

bool TranscriptLineModel::completePending(quint64 key, const QString& text, const QString& translation)
{
    if (key == 0) return false;
    for (int row = m_entries.size() - 1; row >= 0; --row) {
        Entry& e = m_entries[row];
        if (e.key != key) continue;
        if (e.text == e.pendingText && !singleLine(text).isEmpty()) e.text = singleLine(text);
        e.translation = singleLine(translation);
        e.key = 0;
        e.pendingText.clear();
        emit dataChanged(index(row), index(row));
        return true;
    }
    return false;
}

bool TranscriptLineModel::hasTranslation() const
{
    for (const Entry& e : m_entries) {
        if (!e.translation.isEmpty()) return true;
    }
    return false;
}

QString TranscriptLineModel::translationLinesText(const QList<int>& rows, bool withTime) const
{
    QStringList lines;
    for (int row : rows) {
        if (row < 0 || row >= m_entries.size()) continue;
        Entry e = m_entries.at(row);
        if (e.translation.isEmpty()) continue;
        e.text = e.translation;
        lines << lineOf(e, withTime);
    }
    return lines.join(QLatin1Char('\n'));
}

QString TranscriptLineModel::allTranslationText(bool withTime) const
{
    QList<int> rows;
    rows.reserve(m_entries.size());
    for (int i = 0; i < m_entries.size(); ++i) rows << i;
    return translationLinesText(rows, withTime);
}

void TranscriptLineModel::clear()
{
    if (m_entries.isEmpty()) return;
    beginResetModel();
    m_entries.clear();
    endResetModel();
}

bool TranscriptLineModel::setText(int row, const QString& text)
{
    if (row < 0 || row >= m_entries.size()) return false;
    const QString t = singleLine(text);
    if (t.isEmpty() || t == m_entries.at(row).text) return false;
    m_entries[row].text = t;
    emit dataChanged(index(row), index(row));
    return true;
}

bool TranscriptLineModel::setTranslation(int row, const QString& translation)
{
    if (row < 0 || row >= m_entries.size()) return false;
    const QString t = singleLine(translation);
    if (t == m_entries.at(row).translation) return false;
    m_entries[row].translation = t;
    emit dataChanged(index(row), index(row));
    return true;
}

QString TranscriptLineModel::translationAt(int row) const
{
    return row >= 0 && row < m_entries.size() ? m_entries.at(row).translation : QString();
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

QString TranscriptLineModel::serialize() const
{
    QStringList lines;
    lines.reserve(m_entries.size());
    for (const Entry& e : m_entries) {
        lines << lineOf(e, true);
        if (!e.translation.isEmpty()) lines << kTranslationMark + e.translation;
    }
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
        // 紧跟在某一句后面、以 ">> " 开头的行是它的译文
        if (line.startsWith(kTranslationMark) && !entries.isEmpty() && entries.last().time.isValid()
            && entries.last().translation.isEmpty()) {
            entries.last().translation = singleLine(line.mid(kTranslationMark.size()));
            continue;
        }
        const QRegularExpressionMatch m = kPrefixed.match(line);
        const QTime time = m.hasMatch() ? QTime::fromString(m.captured(1), kTimeFormat) : QTime();
        const QString text = time.isValid() ? m.captured(2).trimmed() : line;
        if (!text.isEmpty()) entries.append({ time, text });
    }
    beginResetModel();
    m_entries = std::move(entries);
    endResetModel();
}
