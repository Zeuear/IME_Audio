#pragma once
#include <QPointer>
#include <QWidget>

class QTimer;
class QVariantAnimation;

// @brief 带箭头的气泡提示：取代系统 tooltip（非激活的工具窗口上要等好几秒才出）。
//        文字仍来自控件的 toolTip()，install 之后系统提示被拦下，改由气泡显示。
class BubbleTip : public QWidget
{
    Q_OBJECT
public:
    static void install(QWidget* target);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    explicit BubbleTip();
    static BubbleTip* instance();

    void showFor(QWidget* target);
    void hideTip();

    QPointer<QWidget> m_target;    // 鼠标所在、等待或正在显示提示的控件
    QTimer* m_delay = nullptr;
    QVariantAnimation* m_fade = nullptr;
    QString m_text;
    QSize m_textSize;
    bool m_below = false;          // 气泡在目标下方，箭头朝上
    int m_arrowX = 0;              // 箭头尖在气泡内的横坐标
    qint64 m_hiddenAt = 0;         // 刚从一个按钮移到相邻按钮时直接显示，不再等
};
