#include "DictionaryProfileModel.h"

#include "services/dictionary/DictionaryLanguages.h"

DictionaryProfileModel::DictionaryProfileModel(DictionaryProfiles* profiles, QObject* parent)
    : QAbstractListModel(parent), m_profiles(profiles) {
    reload();
    connect(m_profiles, &DictionaryProfiles::profilesChanged, this, &DictionaryProfileModel::reload);
}

void DictionaryProfileModel::setKind(DictionaryKind kind) {
    m_kind = kind;
    reload();
}

void DictionaryProfileModel::reload() {
    beginResetModel();
    m_items = m_profiles->profiles(m_kind);
    endResetModel();
}

int DictionaryProfileModel::rowOfId(const QString& id) const {
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items[i].id == id) return i;
    }
    return -1;
}

int DictionaryProfileModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : m_items.size();
}

QVariant DictionaryProfileModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() >= m_items.size()) return {};
    const DictionaryProfile& p = m_items.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case NameRole: return p.name;
    case IdRole: return p.id;
    case SubtitleRole: return DictionaryLanguages::subtitle(p);
    case EnabledRole: return p.enabled;
    case RemovableRole: return p.id != DictionaryProfiles::defaultId();
    default: return {};
    }
}

bool DictionaryProfileModel::setData(const QModelIndex& index, const QVariant& value, int role) {
    if (!index.isValid() || role != EnabledRole) return false;
    return m_profiles->setEnabled(m_items.at(index.row()).id, value.toBool());
}

Qt::ItemFlags DictionaryProfileModel::flags(const QModelIndex& index) const {
    return QAbstractListModel::flags(index) & ~Qt::ItemIsEditable;
}
