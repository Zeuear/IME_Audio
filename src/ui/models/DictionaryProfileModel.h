#pragma once
#include <QAbstractListModel>
#include <QList>

#include "services/dictionary/DictionaryProfiles.h"

// @brief 词典配置列表的数据：当前 tab（识别 / 翻译）下的所有配置，勾选即启用。
class DictionaryProfileModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Roles { IdRole = Qt::UserRole + 1, NameRole, SubtitleRole, EnabledRole, RemovableRole };

    DictionaryProfileModel(DictionaryProfiles* profiles, QObject* parent = nullptr);

    void setKind(DictionaryKind kind);
    DictionaryKind kind() const { return m_kind; }
    int rowOfId(const QString& id) const;

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

private:
    void reload();

    DictionaryProfiles* m_profiles;
    DictionaryKind m_kind = DictionaryKind::Recognition;
    QList<DictionaryProfile> m_items;
};
