#pragma once
#include <QPointer>
#include <QWidget>
#include "PanelState.h"

class QTabWidget;

// @brief 横条下方展开的面板：用 tab 切换词典和文件转录两页。
//        页面控件由外部传入（面板接管所有权），面板本身不依赖转录服务。
//        是单独的窗口紧贴在横条下面，横条移到哪它跟到哪：横条是不可激活的窗口，里面的输入框收不到键盘，
//        所以面板不能是横条的一部分。打开时"显示但不激活"，不会把前台窗口切走，
//        用户自己点进输入框时才拿焦点。
class ExpandPanel : public QWidget
{
    Q_OBJECT
public:
    explicit ExpandPanel(QWidget* parent = nullptr);

    void setPages(QWidget* dictionary, QWidget* file);
    // 跟随这个窗口（横条）的位置；面板可见时它移动或改变大小，面板重新摆放
    void attachTo(QWidget* anchor);

    PanelState::Page page() const { return m_page; }
    // None 表示收起
    void showPage(PanelState::Page page);

signals:
    // 面板页面变了（程序切页、用户点 tab、被别处隐藏都会报，每次变化只报一次），横条据此点亮图标
    void pageChanged(PanelState::Page page);

protected:
    void paintEvent(QPaintEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void changeEvent(QEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void retranslateUi();
    void reposition();
    void onTabChanged(int index);

    QTabWidget* m_tabs = nullptr;
    QPointer<QWidget> m_anchor;
    PanelState::Page m_page = PanelState::Page::None;
    bool m_syncingTab = false;   // 程序切 tab 时不当作用户操作再报一次
};
