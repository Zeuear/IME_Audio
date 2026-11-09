#pragma once
#include <QDialog>
#include <QStringList>

class QDialogButtonBox;
class QLabel;
class QPlainTextEdit;

// @brief
// 添加链接的输入框：多行输入，一行一个，实时统计有效和无效行数。
class LinkInputDialog : public QDialog {
    Q_OBJECT
public:
    explicit LinkInputDialog(QWidget* parent = nullptr);

    QStringList links() const { return m_links; }
    QStringList invalidLines() const { return m_invalid; }

protected:
    void changeEvent(QEvent* event) override;

private:
    void retranslateUi();
    void refresh();

    QLabel* m_hint = nullptr;
    QPlainTextEdit* m_edit = nullptr;
    QLabel* m_summary = nullptr;
    QDialogButtonBox* m_buttons = nullptr;
    QStringList m_links;
    QStringList m_invalid;
};
