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
        int blurRadius = 15,        // 模糊半径（影响扩散程度）
        int offsetX = 0,           // 水平偏移
        int offsetY = 2,           // 垂直偏移
        int layers = 10,           // 阴影层数
        qreal opacityFactor = 0.5 // 整体透明度因子
    );

    static void paintGradientShadow(
        QPainter& painter,
        int width,
        int height,
        int shadowRadius,  // 新增：陰影擴散半徑
        QColor shadowColor // 新增：陰影顏色
    );

    static QPixmap drawShadowEffect(
        const QPixmap& sourcePixmap,
        const QColor& baseColor = QColor(0, 0, 0),
        qreal offset = 2.0,
        qreal blurRadius = 6.0,
        qreal devicePixelRatio = 1.0);

};



#endif // PROGRAM_H