#pragma once
#include <QPointer>
#include <QWidget>
#include "PanelState.h"

class QTabWidget;

// @brief 横条下方展开的面板（词典、文件转录两页），页面由外部传入；横条不可激活收不到键盘，故面板是紧贴其下的独立窗口，显示但不激活。
class ExpandPanel : public QWidget
{
    Q_OBJECT
public:
    explicit ExpandPanel(QWidget* parent = nullptr);

    void setPages(QWidget* dictionary, QWidget* file);
    // 跟随这个窗口（横条）的位置；面板可见时它移动或改变大小，面板重新摆放
    void attachTo(QWidget* anchor);

    // 和横条的置顶设置保持一致：横条不置顶时面板也不该盖在别的软件上面
    void setAlwaysOnTop(bool on);

    PanelState::Page page() const { return m_page; }
    // None 表示收起
    void showPage(PanelState::Page page);

signals:
    // 面板页面变了（每次变化只报一次），横条据此点亮图标
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
    bool m_changingFlags = false;   // 改置顶标志时 Qt 会先隐藏再显示，那不是用户收起了面板
};
