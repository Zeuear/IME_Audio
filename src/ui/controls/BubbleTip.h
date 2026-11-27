#pragma once
#include <QPointer>
#include <QWidget>

class QTimer;
class QVariantAnimation;

// @brief 带箭头的气泡提示：取代非激活工具窗口上要等好几秒才出的系统 tooltip。
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

    QPointer<QWidget> m_target;
    QTimer* m_delay = nullptr;
    QVariantAnimation* m_fade = nullptr;
    QString m_text;
    QSize m_textSize;
    bool m_below = false;
    int m_arrowX = 0;
    qint64 m_hiddenAt = 0;         // 刚从一个按钮移到相邻按钮时直接显示，不再等
};
