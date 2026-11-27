#pragma once
#include <QDialog>

#include "services/dictionary/DictionaryProfiles.h"

class QComboBox;
class QDialogButtonBox;
class QLineEdit;

// @brief 新建 / 修改词典配置：名称 + 绑定语言（识别：一种或「不限」；翻译：一对语言，双向生效）。
class DictionaryProfileDialog : public QDialog {
    Q_OBJECT
public:
    DictionaryProfileDialog(DictionaryKind kind, bool editing, QWidget* parent = nullptr);

    void setValues(const QString& name, const QString& langA, const QString& langB);
    QString name() const;
    QString langA() const;
    QString langB() const;

private:
    void updateOk();
    void fillLanguages(QComboBox* combo, bool withAny);

    DictionaryKind m_kind;
    QLineEdit* m_nameEdit = nullptr;
    QComboBox* m_langA = nullptr;
    QComboBox* m_langB = nullptr;
    QDialogButtonBox* m_buttons = nullptr;
};
