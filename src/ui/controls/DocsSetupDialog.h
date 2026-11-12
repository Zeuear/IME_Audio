#pragma once

#include <QDialog>
#include <functional>

class DocsBridge;
QT_BEGIN_NAMESPACE
class QLabel;
class QPushButton;
class QVBoxLayout;
class QWidget;
QT_END_NAMESPACE

// @brief Google 文档的安装与绑定引导。打开时导出扩展；步骤按扩展的连接、绑定状态自动打勾，
// 绑定直接由程序命令扩展完成，用户不用再去 Chrome 里点扩展图标。
class DocsSetupDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DocsSetupDialog(QWidget* parent = nullptr);
    void setBridge(DocsBridge* bridge);

signals:
    void errorOccurred(const QString& title, const QString& cause);

protected:
    void showEvent(QShowEvent* event) override;

private:
    void setupUi();
    QLabel* addHeader(QVBoxLayout* layout, const QString& title);
    QPushButton* addCopyButton(const QString& text, const std::function<QString()>& value);
    void prepareExtension();
    void refresh();
    void showBindError(const QString& reason);

    DocsBridge* m_bridge = nullptr;
    QLabel* m_installStatus = nullptr;
    QWidget* m_installSteps = nullptr;
    QLabel* m_bindStatus = nullptr;
    QLabel* m_bindHint = nullptr;
    QPushButton* m_newDocBtn = nullptr;
    QPushButton* m_bindActiveBtn = nullptr;
    QPushButton* m_stopBtn = nullptr;
};
