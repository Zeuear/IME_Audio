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
    enum class Toggle { Enhance, Translate, Inject };

    explicit ControlBar(QWidget* parent = nullptr);

    QAbstractButton* toggleButton(Toggle which) const;
    QAbstractButton* settingsButton() const;
    QAbstractButton* transcriptButton() const;
    QAbstractButton* dictionaryButton() const;
    QAbstractButton* fileButton() const;
    void setPanelPage(PanelState::Page page);
    void setFileBusy(bool busy);
    QAction* settingsAction() const { return m_settingsAction; }
    QAction* quitAction() const { return m_quitAction; }
    QAction* hideAction() const { return m_hideAction; }
    QAction* alwaysOnTopAction() const { return m_topAction; }
    void setToggleHint(Toggle which, const QString& hint);

    void setAlwaysOnTop(bool on);
    void setWorkflowState(WorkflowState state);

    void showAtSavedPosition();
    void showAt(const QPoint& topLeft);

    bool timerRunning() const;
    QString elapsedText() const;

signals:
    void startRequested();
    void stopRequested();
    void settingsRequested();
    void panelRequested(PanelState::Page page);
    void quitRequested();
    // 只表达意图：隐藏前要先收起展开面板，由宿主一并处理
    void hideRequested();
    void alwaysOnTopToggled(bool on);
    void docsRequested();

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
    QToolButton* m_topBtn = nullptr;
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
    QAction* m_hideAction = nullptr;
    QAction* m_quitAction = nullptr;
    QString m_hints[3];

    bool m_dragging = false;
    QPoint m_dragOffset;
};
