#pragma once
#include <QPointer>
#include <QWidget>
#include "PanelState.h"

class QTabWidget;
class QTimer;

// @brief 横条下方展开的面板（实时转录、词典、文件转录三页），页面由外部传入；横条不可激活收不到键盘，故面板是紧贴其下的独立窗口，显示但不激活。
class ExpandPanel : public QWidget
{
    Q_OBJECT
public:
    explicit ExpandPanel(QWidget* parent = nullptr);

    void setPages(QWidget* transcript, QWidget* dictionary, QWidget* file);
    void attachTo(QWidget* anchor);

    void releaseSharedPages();
    void takeBackSharedPages(QWidget* transcript, QWidget* dictionary, QWidget* file);

    void setAlwaysOnTop(bool on);

    PanelState::Page page() const { return m_page; }
    void showPage(PanelState::Page page);

signals:
    void pageChanged(PanelState::Page page);

protected:
    void paintEvent(QPaintEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void changeEvent(QEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    class BorderOverlay;
    void retranslateUi();
    void reposition();
    void onTabChanged(int index);
    Qt::Edges resizeEdgesAt(const QPoint& pos) const;
    static int indexOf(PanelState::Page page);
    static PanelState::Page pageAt(int index);

    QTabWidget* m_tabs = nullptr;
    QWidget* m_border = nullptr;
    QTimer* m_sizeSaveTimer = nullptr;
    QSize m_savedSize;
    QPointer<QWidget> m_anchor;
    PanelState::Page m_page = PanelState::Page::None;
    bool m_syncingTab = false;   // 程序切 tab 时不当作用户操作再报一次
    bool m_changingFlags = false;   // 改置顶标志时 Qt 会先隐藏再显示，那不是用户收起了面板
};
