#ifndef APPLESHADOWEFFECT_H
#define APPLESHADOWEFFECT_H

#include <QGraphicsEffect>
#include <QColor>
#include <QPointF>
#include <QGraphicsOpacityEffect>
#include <QPixmap>
#include <QDebug>

class AppleShadowEffect : public QGraphicsEffect
{
    Q_OBJECT
    Q_PROPERTY(qreal opacity READ opacity WRITE setOpacity)
public:
    explicit AppleShadowEffect(QObject *parent = nullptr);

    void setBlurRadius(qreal radius) { m_blurRadius = radius; update(); }
    qreal blurRadius() const { return m_blurRadius; }

    void setInnerDistance(int radius) { m_innerDistance = radius; update(); }
    qreal innerDistance() const { return m_innerDistance; }

    void setOffset(const QPointF &offset) { m_offset = offset; update(); }
    QPointF offset() const { return m_offset; }

    void setColor(const QColor &color) { m_color = color; update(); }
    QColor color() const { return m_color; }

    void setOpacity(qreal opacity) { m_opacity = opacity; update(); }
    qreal opacity() const { return m_opacity; }

protected:
    void draw(QPainter *painter) override;
    QRectF boundingRectFor(const QRectF &rect) const override;

private:
    QPixmap updateShadow(const QRectF& sourceRect);  

    int m_innerDistance;
    qreal m_blurRadius;
    QPointF m_offset;
    QColor m_color;
    qreal m_opacity;
    QPixmap m_shadowPixmap;  
    QRectF m_lastSourceRect;  
};



#include <QWidget>
#include <QMouseEvent>
class ShadowDemoWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ShadowDemoWidget(QWidget* parent = nullptr);
    ~ShadowDemoWidget();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;

private:
    AppleShadowEffect* m_shadowEffect;
    QPoint m_dragPosition; 
};



#endif // APPLESHADOWEFFECT_H