#include "painttool.h"
#include "qmath.h"
#include <QPainterPath>
void PaintTool::paintShadow(
    QPainter& painter,
    int width,
    int height,
    QColor shadowColor,
    QPainterPath clipPath,
    int blurRadius,        // 模糊半径
    int offsetX,           // 水平偏移
    int offsetY,           // 垂直偏移
    int layers,            // 阴影层数
    qreal opacityFactor    // 整体透明度因子
)
{
    // 绘制阴影
    //int expand = 8;
    //int edge = 10;
    //int maxLayers = 10;

    //int startAlpha = shadowColor.alpha();
    //for (int i = 0; i < maxLayers; i++)
    //{
        //shadowColor.setAlpha(startAlpha - int(std::sqrt(i) * 20));
        //painter.setPen(shadowColor);

        //// 使用线性衰减：alpha 从 startAlpha 线性减少到 0
        //qreal factor = 1.0 - qreal(i) / (maxLayers - 1); // 0 到 1 的线性插值
        //shadowColor.setAlpha(int(startAlpha * factor));

        //painter.setPen(Qt::NoPen); // 不使用笔，填充整个矩形
        //painter.setBrush(QBrush(shadowColor));

    //    int distance = i + 1;

    //    QRect shadowRect = QRect(edge - distance, edge - distance,
    //        width - (edge - distance) * 2,
    //        height - (edge - distance) * 2);

    //    painter.drawRoundedRect(shadowRect, 15, 15);
    //}
    // 保存画家状态，以便后续恢复
    painter.save();


    painter.setRenderHint(QPainter::Antialiasing); // 确保抗锯齿
    // 如果提供了裁剪路径，设置裁剪
    if (!clipPath.isEmpty()) {
        painter.setClipPath(clipPath);
    }

    int startAlpha = shadowColor.alpha() * opacityFactor; // 应用整体透明度因子
    qreal sigma = blurRadius / 3.0; // 将模糊半径转换为标准差，3 是一个经验值


    for (int i = 0; i < layers; i++) {
        // 计算当前层的距离（从中心到边缘）
        qreal distance = qreal(i) - (layers - 1) / 2.0; // 中心化

        // 高斯函数：e^(-distance^2 / (2 * sigma^2))
        qreal gaussian = qExp(-distance * distance / (2 * sigma * sigma));

        // 设置当前层的 alpha 值
        shadowColor.setAlpha(int(startAlpha * gaussian));

        painter.setPen(Qt::NoPen);
        painter.setBrush(QBrush(shadowColor));

        // 计算阴影矩形，考虑偏移和扩展
        int expand = blurRadius / 2; // 扩展量基于模糊半径
        int edge = expand + qMax(qAbs(offsetX), qAbs(offsetY)); // 确保覆盖偏移

        QRect shadowRect = QRect(
            edge + offsetX - i,  // 左上角 X，包含偏移
            edge + offsetY - i,  // 左上角 Y，包含偏移
            width - (edge - i) * 2,  // 宽度
            height - (edge - i) * 2  // 高度
        );

        // 绘制圆角矩形
        painter.drawRoundedRect(shadowRect, 15, 15); // 圆角半径保持为 15，可作为参数进一步灵活化

    }

    painter.restore();
}


void PaintTool::paintGradientShadow(
    QPainter& painter,
    int width,
    int height,
    int shadowRadius,  // 新增：陰影擴散半徑
    QColor shadowColor // 新增：陰影顏色
)
{
    // 啟用抗鋸齒以獲得更平滑的邊緣
    painter.setRenderHint(QPainter::Antialiasing);

    // 绘制阴影
    int offsetX = 0;       // 新增：陰影X偏移
    int offsetY = 15;       // 新增：陰影Y偏移
    // 定義繪製區域（考慮偏移）
    int edge = 0; // 邊距
    QRect shadowRect(edge, edge, width - 2 * edge, height - 2 * edge);


    QColor edgeColor = shadowColor.lighter(100);
    edgeColor.setAlpha(0);

    // 使用 QRadialGradient 創建放射狀漸變陰影
    QRadialGradient gradient(shadowRect.center() + QPoint(offsetX, offsetY), shadowRect.width() / 2 + shadowRadius);
    gradient.setColorAt(0.3, shadowColor); // 中心顏色（較濃）
    gradient.setColorAt(0.9, edgeColor); // 邊緣透明

    // 設置畫筆和填充
    painter.setPen(Qt::NoPen); // 無邊框
    painter.setBrush(gradient);

    // 繪製圓角矩形陰影
    painter.drawRoundedRect(shadowRect, 15, 15);
}



QPixmap PaintTool::drawShadowEffect(const QPixmap& sourcePixmap, const QColor& baseColor, qreal offset, qreal blurRadius, qreal devicePixelRatio)
{
    // Check if source pixmap is valid
    if (sourcePixmap.isNull()) {
        return QPixmap();
    }

    // Calculate output pixmap size with blur radius
    QSize sourceSize = sourcePixmap.size();
    QRectF sourceRect(0, 0, sourceSize.width(), sourceSize.height());
    QSize pixmapSize = sourceRect.size().toSize() * devicePixelRatio;

    // Create output pixmap
    QPixmap outputPixmap(pixmapSize);
    outputPixmap.setDevicePixelRatio(devicePixelRatio);
    outputPixmap.fill(Qt::transparent);

    QPainter painter(&outputPixmap);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform);

    // Derive shadow colors
    QColor edgeColor = baseColor.darker(120); // Edge darkening
    edgeColor.setAlpha(255); // Soft opacity
    QColor midColor = baseColor.darker(105); // Mid-tone
    midColor.setAlpha(150); // Smooth transition
    QColor centerColor = baseColor;
    centerColor.setAlpha(0); // Transparent center

    // Create radial gradient for shadow
    QRadialGradient shadowGradient(sourceRect.center(), sourceRect.width()/2);
    shadowGradient.setColorAt(0.0, centerColor); // Transparent center
    shadowGradient.setColorAt(0.8, centerColor); // Mid-tone transition
    shadowGradient.setColorAt(0.9, midColor); // Mid-tone transition
    shadowGradient.setColorAt(1.0, edgeColor); // Fade to transparent

    //painter.setBrush(QColor(255, 255, 255, 255));
    //painter.drawRect(sourceRect);

    // Draw source pixmap on top
    painter.drawPixmap(0, 0, sourcePixmap.scaled(sourceSize * devicePixelRatio, Qt::KeepAspectRatio, Qt::SmoothTransformation));

    // Draw shadow
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);

    painter.setPen(Qt::NoPen);
    painter.setBrush(shadowGradient);
    //painter.drawRect(sourceRect);

    return outputPixmap;
}