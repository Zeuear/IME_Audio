#ifndef PAINTTOOL_H
#define PAINTTOOL_H

#include <QColor>
#include <QPainter>
#include <QVector>
#include <QPainterPath>

class PaintTool
{
public:
	static void paintShadow(
        QPainter& painter,
        int width,
        int height,
        QColor shadowColor,
        QPainterPath clipPath = QPainterPath(),
        int blurRadius = 15,       
        int offsetX = 0,          
        int offsetY = 2,          
        int layers = 10,         
        qreal opacityFactor = 0.5
    );

    static void paintGradientShadow(
        QPainter& painter,
        int width,
        int height,
        int shadowRadius, 
        QColor shadowColor
    );

    static QPixmap drawShadowEffect(
        const QPixmap& sourcePixmap,
        const QColor& baseColor = QColor(0, 0, 0),
        qreal offset = 2.0,
        qreal blurRadius = 6.0,
        qreal devicePixelRatio = 1.0);

};



#endif // PROGRAM_H