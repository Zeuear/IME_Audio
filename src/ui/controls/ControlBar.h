#pragma once
#include <QPoint>
#include <QString>
#include <QWidget>
#include "services/WorkflowManager.h"
#include "PanelState.h"

class AppleShadowEffect;
class QAbstractButton;
class QAction;
class QContextMenuEvent;
class QLabel;
class QMenu;
class QTimer;
class QToolButton;

// @brief 常驻置顶的图标横条：开始 / 停止、开关、面板入口。不抢焦点，可拖动，位置记在配置里。
class ControlBar : public QWidget
{
    Q_OBJECT
public:
    // 开关类图标，状态由宿主用 ToggleLink 连到设置里已有的控件，横条自己不保存
    enum class Toggle { Enhance, Translate, Inject };

    explicit ControlBar(QWidget* parent = nullptr);

    QAbstractButton* toggleButton(Toggle which) const;
    QAbstractButton* settingsButton() const;
    QAbstractButton* transcriptButton() const;
    QAbstractButton* dictionaryButton() const;
    QAbstractButton* fileButton() const;
    // 点击只发 panelRequested，面板状态由宿主决定后用它回写
    void setPanelPage(PanelState::Page page);
    void setFileBusy(bool busy);
    QAction* settingsAction() const { return m_settingsAction; }
    QAction* quitAction() const { return m_quitAction; }
    QAction* alwaysOnTopAction() const { return m_topAction; }
    // 在名称后追加一行说明；传空串去掉
    void setToggleHint(Toggle which, const QString& hint);

    void setAlwaysOnTop(bool on);
    // Google 文档输出状态：未绑定时菜单项置灰，绑定或断开时可点它停止输出
    enum class DocsLink { Unbound, Bound, Lost };
    void setDocsLink(DocsLink link, const QString& title);
    void setWorkflowState(WorkflowState state);

    // 读取保存的位置并校正到屏幕内后显示（不激活）
    void showAtSavedPosition();

    // 录音计时：会话进行中每秒走一次，结束后归零
    bool timerRunning() const;
    QString elapsedText() const;

signals:
    void startRequested();
    void stopRequested();
    void settingsRequested();
    void panelRequested(PanelState::Page page);
    // 有的桌面没有托盘，退出入口放在横条的右键菜单
    void quitRequested();
    void alwaysOnTopToggled(bool on);
    void docsReleaseRequested();
    // 导出随程序附带的 Chrome 扩展并打开安装说明
    void docsExtensionRequested();

protected:
    void paintEvent(QPaintEvent* event) override;
    void changeEvent(QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    void retranslateUi();
    void refreshIcons();
    void refreshToggleTips();
    void refreshTimerLabel();
    void tick();
    QToolButton* button(Toggle which) const;

    QToolButton* m_startBtn = nullptr;
    QToolButton* m_stopBtn = nullptr;
    QToolButton* m_enhanceBtn = nullptr;
    QToolButton* m_translateBtn = nullptr;
    QToolButton* m_injectBtn = nullptr;
    QToolButton* m_transcriptBtn = nullptr;
    QToolButton* m_dictionaryBtn = nullptr;
    QToolButton* m_fileBtn = nullptr;
    QToolButton* m_settingsBtn = nullptr;
    QLabel* m_timerLabel = nullptr;
    AppleShadowEffect* m_shadow = nullptr;
    QTimer* m_ticker = nullptr;
    qint64 m_elapsedSec = 0;
    bool m_fileBusy = false;
    QMenu* m_menu = nullptr;
    QAction* m_settingsAction = nullptr;
    QAction* m_topAction = nullptr;
    QAction* m_docsAction = nullptr;
    QAction* m_docsInstallAction = nullptr;
    DocsLink m_docsLink = DocsLink::Unbound;
    QString m_docsTitle;
    QAction* m_quitAction = nullptr;
    QString m_hints[3];   // 按 Toggle 的顺序

    bool m_dragging = false;
    QPoint m_dragOffset;   // 按下时鼠标相对窗口左上角的偏移
};
