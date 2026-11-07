#pragma once
#include <QPoint>
#include <QWidget>
#include "services/WorkflowManager.h"

class QToolButton;

// @brief 横条：常驻置顶的小窗，一键开始 / 停止听写。不抢焦点，点击它不会把前台窗口切走。
//        只负责显示和发信号，不依赖任何服务；按钮是否可点由工作流状态决定（ControlBarState）。
//        可拖动，位置记在配置文件里，下次启动回到原处。
class ControlBar : public QWidget
{
    Q_OBJECT
public:
    explicit ControlBar(QWidget* parent = nullptr);

    void setWorkflowState(WorkflowState state);

    // 读取保存的位置并校正到当前屏幕内后显示（不激活）
    void showAtSavedPosition();

signals:
    void startRequested();
    void stopRequested();

protected:
    void paintEvent(QPaintEvent* event) override;
    void changeEvent(QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    void retranslateUi();
    void refreshIcons();

    QToolButton* m_startBtn = nullptr;
    QToolButton* m_stopBtn = nullptr;

    bool m_dragging = false;
    QPoint m_dragOffset;   // 按下时鼠标相对窗口左上角的偏移
};
