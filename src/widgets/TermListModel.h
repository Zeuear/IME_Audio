#pragma once
#include <QAbstractListModel>
#include <QSet>
#include "../TermsLibraryManager.h"

// @brief 词典卡片列表的数据：一行一条词条（听成 → 写成），另外记录多选模式下勾选了哪些行。
//        勾选状态放在模型里而不是 QItemSelectionModel：批量启用会重建模型，勾选要保留下来。
class TermListModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Roles { HeardRole = Qt::UserRole + 1, CorrectRole, EnabledRole, CheckedRole };

    explicit TermListModel(TermsLibraryManager* manager, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    // EditRole 的值是 QStringList{ 听成原文, 写成 }
    bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    bool addTerm(const QString& heardText, const QString& correct);

    bool isChecked(int row) const { return m_checked.contains(row); }
    void setChecked(const QSet<int>& rows);
    QList<int> checkedRows() const;

signals:
    void errorOccurred(const QString& cause);
    void checkedChanged();

private:
    // 返回出错原因，合法时为空
    QString validate(const QStringList& heard, const QString& correct, int row) const;

    TermsLibraryManager* m_manager;
    QSet<int> m_checked;
    int m_rowCount = 0;
};
