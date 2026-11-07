#pragma once
#include <QWidget>
#include <memory>

class QLabel;
class QProgressBar;
class QPushButton;
class QTabWidget;
class QTextEdit;
class UpdateManager;
class UpdateStatusModel;

// @brief 设置窗口里的"关于"页：版本、检查更新（状态文字、下载进度）、关于 Qt、简介与更新说明。
//        检查更新的状态逻辑在 UpdateStatusModel 里，本控件只负责把它和 UpdateManager 的信号、界面连起来。
class AboutPage : public QWidget
{
    Q_OBJECT
public:
    explicit AboutPage(QWidget* parent = nullptr);
    ~AboutPage() override;

    void setUpdateManager(UpdateManager* updates);

    // 用户点了检查更新或下载，结果由本页显示；宿主据此不再弹启动自动检查用的那个对话框
    bool isUserDriven() const;

protected:
    void changeEvent(QEvent* event) override;

private:
    void setupUi();
    void retranslateUi();
    void refresh();
    QString loadChangelog() const;

    UpdateManager* m_updates = nullptr;
    std::unique_ptr<UpdateStatusModel> m_status;

    QLabel* m_iconLabel = nullptr;
    QLabel* m_titleLabel = nullptr;
    QLabel* m_versionLabel = nullptr;
    QPushButton* m_checkBtn = nullptr;
    QPushButton* m_installBtn = nullptr;
    QPushButton* m_aboutQtBtn = nullptr;
    QLabel* m_statusLabel = nullptr;
    QProgressBar* m_progress = nullptr;
    QTabWidget* m_tabs = nullptr;
    QTextEdit* m_descriptionText = nullptr;
    QTextEdit* m_changelogText = nullptr;
};
