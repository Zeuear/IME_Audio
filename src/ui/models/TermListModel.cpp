#include "TermListModel.h"
#include <algorithm>

TermListModel::TermListModel(TermsLibraryManager* manager, QObject* parent)
    : QAbstractListModel(parent), m_manager(manager)
{
    m_rowCount = rowCount();
    connect(m_manager, &TermsLibraryManager::termsReloaded, this, [this]() {
        beginResetModel();
        endResetModel();
        // 信号发出时词条已经改完，只能和上次记下的行数比。
        // 行数变了（增删、重新加载）行号就对不上了，勾选作废；只改内容时保留
        if (rowCount() != m_rowCount && !m_checked.isEmpty()) {
            m_checked.clear();
            emit checkedChanged();
        }
        m_rowCount = rowCount();
    });
}

int TermListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_manager->items().size();
}

QVariant TermListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= rowCount()) return {};
    const TermItem& item = m_manager->items().at(index.row());

    switch (role) {
    case Qt::DisplayRole:   // 供搜索过滤
        return item.heard().join(' ') + ' ' + item.correct;
    case HeardRole:   return item.heard();
    case CorrectRole: return item.correct;
    case EnabledRole: return item.enabled;
    case CheckedRole: return m_checked.contains(index.row());
    default: return {};
    }
}

bool TermListModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!index.isValid() || role != Qt::EditRole) return false;
    const QStringList parts = value.toStringList();
    const QStringList heard = TermsLibraryManager::splitWords(parts.value(0));
    const QString correct = parts.value(1).trimmed();

    TermItem item = m_manager->items().at(index.row());
    if (item.heard() == heard && item.correct == correct) return false;

    if (const QString err = validate(heard, correct, index.row()); !err.isEmpty()) {
        emit errorOccurred(err);
        return false;
    }
    item.setHeard(heard);
    item.correct = correct;
    m_manager->updateItem(index.row(), item);
    return true;
}

Qt::ItemFlags TermListModel::flags(const QModelIndex& index) const
{
    return QAbstractListModel::flags(index) | Qt::ItemIsEditable;
}

bool TermListModel::addTerm(const QString& heardText, const QString& correct)
{
    const QStringList heard = TermsLibraryManager::splitWords(heardText);
    if (const QString err = validate(heard, correct.trimmed(), -1); !err.isEmpty()) {
        emit errorOccurred(err);
        return false;
    }
    TermItem item;
    item.setHeard(heard);
    item.correct = correct.trimmed();
    m_manager->addItem(item);
    return true;
}

QString TermListModel::validate(const QStringList& heard, const QString& correct, int row) const
{
    if (correct.isEmpty()) return tr("\"写成\"不能为空");
    if (const QString dup = m_manager->duplicateHeard(heard, row); !dup.isEmpty()) {
        return tr("\"%1\" 已在其它词条里，不能重复").arg(dup);
    }
    return {};
}

void TermListModel::setChecked(const QSet<int>& rows)
{
    if (rows == m_checked) return;
    m_checked = rows;
    if (rowCount() > 0) emit dataChanged(index(0), index(rowCount() - 1), { CheckedRole });
    emit checkedChanged();
}

QList<int> TermListModel::checkedRows() const
{
    QList<int> rows(m_checked.begin(), m_checked.end());
    std::sort(rows.begin(), rows.end());
    return rows;
}
