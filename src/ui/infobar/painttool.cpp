#include "painttool.h"
#include "qmath.h"
#include <QPainterPath>
void PaintTool::paintShadow(
    QPainter& painter,
    int width,
    int height,
    QColor shadowColor,
    QPainterPath clipPath,
    int blurRadius,       
    int offsetX,         
    int offsetY,          
    int layers,           
    qreal opacityFactor    
)
{
    painter.save();


    painter.setRenderHint(QPainter::Antialiasing); 
    if (!clipPath.isEmpty()) {
        painter.setClipPath(clipPath);
    }

    int startAlpha = shadowColor.alpha() * opacityFactor;
    qreal sigma = blurRadius / 3.0;


    for (int i = 0; i < layers; i++) {
        qreal distance = qreal(i) - (layers - 1) / 2.0;

        qreal gaussian = qExp(-distance * distance / (2 * sigma * sigma));

        shadowColor.setAlpha(int(startAlpha * gaussian));

        painter.setPen(Qt::NoPen);
        painter.setBrush(QBrush(shadowColor));

        int expand = blurRadius / 2;
        int edge = expand + qMax(qAbs(offsetX), qAbs(offsetY)); 

        QRect shadowRect = QRect(
            edge + offsetX - i, 
            edge + offsetY - i,  
            width - (edge - i) * 2, 
            height - (edge - i) * 2 
        );

        painter.drawRoundedRect(shadowRect, 15, 15); 

    }

    painter.restore();
}


void PaintTool::paintGradientShadow(
    QPainter& painter,
    int width,
    int height,
    int shadowRadius, 
    QColor shadowColor 
)
{
    painter.setRenderHint(QPainter::Antialiasing);

    int offsetX = 0;   
    int offsetY = 15;       
    int edge = 0; 
    QRect shadowRect(edge, edge, width - 2 * edge, height - 2 * edge);


    QColor edgeColor = shadowColor.lighter(100);
    edgeColor.setAlpha(0);

    QRadialGradient gradient(shadowRect.center() + QPoint(offsetX, offsetY), shadowRect.width() / 2 + shadowRadius);
    gradient.setColorAt(0.3, shadowColor); 
    gradient.setColorAt(0.9, edgeColor); 


    painter.setPen(Qt::NoPen); 
    painter.setBrush(gradient);


    painter.drawRoundedRect(shadowRect, 15, 15);
}



QPixmap PaintTool::drawShadowEffect(const QPixmap& sourcePixmap, const QColor& baseColor, qreal offset, qreal blurRadius, qreal devicePixelRatio)
{
    if (sourcePixmap.isNull()) {
        return QPixmap();
    }

    QSize sourceSize = sourcePixmap.size();
    QRectF sourceRect(0, 0, sourceSize.width(), sourceSize.height());
    QSize pixmapSize = sourceRect.size().toSize() * devicePixelRatio;

    QPixmap outputPixmap(pixmapSize);
    outputPixmap.setDevicePixelRatio(devicePixelRatio);
    outputPixmap.fill(Qt::transparent);

    QPainter painter(&outputPixmap);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);

    QColor edgeColor = baseColor.darker(120);
    edgeColor.setAlpha(255);
    QColor midColor = baseColor.darker(105);
    midColor.setAlpha(150);
    QColor centerColor = baseColor;
    centerColor.setAlpha(0);

    QRadialGradient shadowGradient(sourceRect.center(), sourceRect.width()/2);
    shadowGradient.setColorAt(0.0, centerColor);
    shadowGradient.setColorAt(0.8, centerColor);
    shadowGradient.setColorAt(0.9, midColor);
    shadowGradient.setColorAt(1.0, edgeColor);


    painter.drawPixmap(0, 0, sourcePixmap.scaled(sourceSize * devicePixelRatio, Qt::KeepAspectRatio, Qt::SmoothTransformation));

    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);

    painter.setPen(Qt::NoPen);
    painter.setBrush(shadowGradient);

    return outputPixmap;
}