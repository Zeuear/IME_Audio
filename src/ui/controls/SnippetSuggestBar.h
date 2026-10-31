#ifndef SNIPPETSUGGESTBAR_H
#define SNIPPETSUGGESTBAR_H

#include <QString>
#include <QStringList>
#include <QRect>
#include <QRectF>
#include <QSize>
#include <QVector>
#include <QTimer>
#include <QWidget>

class QVariantAnimation;

// @brief 悬浮窗旁的指令入口
class SnippetSuggestBar : public QWidget {
    Q_OBJECT

public:
    struct Suggestion {
        QString id;
        QString label;
        QString tooltip;
    };

    explicit SnippetSuggestBar(QWidget *parent = nullptr);

    // 建议的条目没变时不重建按钮，也不重置自动隐藏计时
    void setSuggestions(const QVector<Suggestion>& suggestions);
    void placeNear(const QRect& anchor, bool below);
    void dismiss();

signals:
    void snippetClicked(const QString& id);
    // 点击或超时收起时带出这批条目的 id，调用方据此在同一句话里不再重复提示
    void closed(const QStringList& ids);
    void visibilityChanged(bool visible);

protected:
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    bool event(QEvent *event) override;

private:
    // 按钮是自绘的，不是 QPushButton：位置大小固定，展开时被底板的轮廓逐步揭开
    struct Item {
        QString id;
        QString label;
        QString tooltip;
        QRectF finalRect;   // 展开后在按钮栏里的位置（栏内坐标）
    };

    void closeAndRemember();
    void setExpanded(bool expanded);
    void applyPlacement();    // 按锚点定位窗口并设回收起遮罩，只在收起且静止时调用
    QRect collapsedRect() const;
    QRectF buttonRect(int i) const;
    int buttonAt(const QPoint& pos) const;

    QVector<Item> m_items;
    QVector<Suggestion> m_pending;  // 鼠标在栏上时到达的新提示，收起后再显示
    QSize m_full;                   // 完全展开后的窗口尺寸
    int m_hover = -1;
    QVariantAnimation* m_anim = nullptr;
    qreal m_progress = 0;           // 0=圆形箭头，1=完整按钮栏
    QStringList m_currentIds;
    QTimer m_hideTimer;
    QTimer m_collapseTimer;
    QRect m_anchor;       // placeNear 记下的最新锚点，展开/动画期间先存着不生效
    bool m_anchorBelow = false;
    bool m_below = false; // 当前生效的方向，绘制用
    bool m_expanded = false;
};

#endif // SNIPPETSUGGESTBAR_H
