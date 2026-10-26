#include "DictationHistoryModel.h"
#include <QStringList>

DictationHistoryModel::DictationHistoryModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int DictationHistoryModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_entries.size();
}

QVariant DictationHistoryModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= m_entries.size()) return {};
    const Entry& e = m_entries.at(index.row());
    if (role == Qt::DisplayRole || role == Qt::ToolTipRole) return e.text;
    if (role == TimeRole) return e.time.toString(QStringLiteral("hh:mm:ss"));
    return {};
}

void DictationHistoryModel::add(const QString& text, const QDateTime& time)
{
    const QString t = text.trimmed();
    if (t.isEmpty()) return;
    beginInsertRows({}, 0, 0);
    m_entries.prepend({ time, t });
    endInsertRows();
    if (m_entries.size() > kMaxEntries) {
        beginRemoveRows({}, kMaxEntries, m_entries.size() - 1);
        m_entries.resize(kMaxEntries);
        endRemoveRows();
    }
}

void DictationHistoryModel::clear()
{
    if (m_entries.isEmpty()) return;
    beginResetModel();
    m_entries.clear();
    endResetModel();
}

QString DictationHistoryModel::allText() const
{
    QStringList lines;
    lines.reserve(m_entries.size());
    for (auto it = m_entries.crbegin(); it != m_entries.crend(); ++it) lines << it->text;
    return lines.join(QLatin1Char('\n'));
}

QString DictationHistoryModel::textAt(int row) const
{
    return row >= 0 && row < m_entries.size() ? m_entries.at(row).text : QString();
}
