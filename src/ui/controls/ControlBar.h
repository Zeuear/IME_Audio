#pragma once
#include <QPoint>
#include <QString>
#include <QWidget>
#include "services/WorkflowManager.h"
#include "PanelState.h"

class QAbstractButton;
class QAction;
class QContextMenuEvent;
class QMenu;
class QToolButton;

// @brief 横条：常驻置顶的小窗，一键开始 / 停止听写。不抢焦点，点击它不会把前台窗口切走。
//        只负责显示和发信号，不依赖任何服务；按钮是否可点由工作流状态决定（ControlBarState）。
//        可拖动，位置记在配置文件里，下次启动回到原处。
class ControlBar : public QWidget
{
    Q_OBJECT
public:
    // 开关类图标：点一下开启（图标变强调色），再点关闭。
    // 横条不保存它们的状态，由宿主用 ToggleLink 连到设置里已有的控件
    enum class Toggle { Enhance, Translate, Inject };

    explicit ControlBar(QWidget* parent = nullptr);

    QAbstractButton* toggleButton(Toggle which) const;
    // 设置入口（打开主窗口）和右键菜单，宿主只需接信号
    QAbstractButton* settingsButton() const;
    // 转录内容图标：点一下打开稿子窗口，再点关闭；窗口开着时图标高亮。
    // 窗口被别处关掉时宿主用 setTranscriptOpen(false) 复位，这样不会再发一次信号
    QAbstractButton* transcriptButton() const;
    void setTranscriptOpen(bool open);
    // 词典、文件转录图标：点击只发 panelRequested，面板怎么变由宿主决定（PanelState），
    // 宿主再用 setPanelPage 点亮对应图标（不会再发信号）
    QAbstractButton* dictionaryButton() const;
    QAbstractButton* fileButton() const;
    void setPanelPage(PanelState::Page page);
    // 文件转录进行中：悬停提示里注明，面板收着时用户也知道它还在跑
    void setFileBusy(bool busy);
    QAction* settingsAction() const { return m_settingsAction; }
    QAction* quitAction() const { return m_quitAction; }
    // 在名称后追加一行说明（例如"需要先开启 AI 增强"）；传空串去掉
    void setToggleHint(Toggle which, const QString& hint);

    // 设置里的"横条置顶"开关；关掉后横条不再盖在别的软件上面
    void setAlwaysOnTop(bool on);

    void setWorkflowState(WorkflowState state);

    // 读取保存的位置并校正到当前屏幕内后显示（不激活）
    void showAtSavedPosition();

signals:
    void startRequested();
    void stopRequested();
    void settingsRequested();
    void transcriptToggled(bool open);
    void panelRequested(PanelState::Page page);
    // 横条是程序的常驻入口，退出不能只靠托盘（有的桌面没有托盘）
    void quitRequested();

protected:
    void paintEvent(QPaintEvent* event) override;
    void changeEvent(QEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    void retranslateUi();
    void refreshIcons();
    void refreshToggleTips();
    QToolButton* button(Toggle which) const;

    QToolButton* m_startBtn = nullptr;
    QToolButton* m_stopBtn = nullptr;
    QToolButton* m_enhanceBtn = nullptr;
    QToolButton* m_translateBtn = nullptr;
    QToolButton* m_injectBtn = nullptr;
    QToolButton* m_transcriptBtn = nullptr;
    QToolButton* m_dictionaryBtn = nullptr;
    QToolButton* m_fileBtn = nullptr;
    bool m_fileBusy = false;
    QToolButton* m_settingsBtn = nullptr;
    QMenu* m_menu = nullptr;
    QAction* m_settingsAction = nullptr;
    QAction* m_quitAction = nullptr;
    QString m_hints[3];   // 按 Toggle 的顺序

    bool m_dragging = false;
    QPoint m_dragOffset;   // 按下时鼠标相对窗口左上角的偏移
};
