#pragma once

#include <QDialog>

#include "utils/PlatformPermissions.h"

QT_BEGIN_NAMESPACE
class QGridLayout;
class QLabel;
class QPushButton;
class QTimer;
QT_END_NAMESPACE

// @brief
// 启动时的授权引导（目前只有 macOS 需要）：列出麦克风与辅助功能的授权状态，逐项引导授权，
// 全部授权后由用户点「立即重启」使其生效。
// 是否弹出按当前授权状态判断而不是"首次安装"标记：签名不变时升级后授权仍在，引导不会出现；
// 授权被关掉、换了签名或换了电脑时又会自动出现。Windows / Linux 上授权都是 NotRequired，永不出现。
class PermissionGuideDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PermissionGuideDialog(QWidget* parent = nullptr);
    static bool isNeeded();

private:
    struct Row {
        QLabel* status = nullptr;
        QPushButton* button = nullptr;
    };

    void setupUi();
    Row addRow(QGridLayout* grid, int row, const QString& title, const QString& description);
    void refresh();
    void onMicrophoneClicked();
    void onAccessibilityClicked();
    void setStatus(QLabel* label, PlatformPermissions::Status status);

    Row m_mic;
    Row m_accessibility;
    QLabel* m_accessibilityHint = nullptr;
    QLabel* m_summaryLabel = nullptr;
    QPushButton* m_restartBtn = nullptr;
    QTimer* m_pollTimer = nullptr;
    bool m_accessibilityPrompted = false;   // 系统的辅助功能提示框每个进程只弹一次，之后直接打开系统设置
};
