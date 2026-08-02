#pragma once

#include <QDialog>

QT_BEGIN_NAMESPACE
class QLabel;
class QTextEdit;
class QPushButton;
QT_END_NAMESPACE

class AboutDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AboutDialog(QWidget* parent = nullptr);
    ~AboutDialog() override = default;

private:
    void setupUi();
    QString loadChangelog() const;

private:
    QLabel* m_iconLabel = nullptr;
    QLabel* m_titleLabel = nullptr;
    QLabel* m_versionLabel = nullptr;
    QTextEdit* m_descriptionText = nullptr;
    QTextEdit* m_changelogText = nullptr;
    QPushButton* m_closeButton = nullptr;
};