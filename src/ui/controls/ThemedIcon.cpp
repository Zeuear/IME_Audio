#include "ui/controls/ThemedIcon.h"

#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QPixmap>
#include <QSvgRenderer>
#include <cmath>

#include "utils/Logger.h"

namespace {

QPixmap renderSvg(const QByteArray& svg, int logicalSize) {
    const qreal dpr = std::ceil(qApp->devicePixelRatio());
    const int px = static_cast<int>(logicalSize * dpr);

    // 直接用 QSvgRenderer，不依赖 qsvg 图片格式插件
    QSvgRenderer renderer(svg);
    if (!renderer.isValid()) {
        LOG_WARN("Themed icon render failed: invalid SVG");
        return {};
    }
    QImage image(px, px, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    {
        QPainter painter(&image);
        renderer.render(&painter);
    }
    QPixmap pixmap = QPixmap::fromImage(image);
    pixmap.setDevicePixelRatio(dpr);
    return pixmap;
}

} // namespace

QIcon themedIcon(const QString& resource, const QColor& color, int logicalSize) {
    QFile file(QStringLiteral(":/") + resource);
    if (!file.open(QIODevice::ReadOnly)) {
        LOG_WARN(QString("Themed icon not found: %1").arg(resource));
        return {};
    }
    const QByteArray raw = file.readAll();

    const QPixmap normal = renderSvg(QByteArray(raw).replace("currentColor", color.name(QColor::HexRgb).toUtf8()),
                                     logicalSize);
    if (normal.isNull()) return {};

    // 禁用态用同图半透明：SVG 不认 #AARRGGBB，所以在位图上做
    QPixmap disabled(normal.size());
    disabled.setDevicePixelRatio(normal.devicePixelRatio());
    disabled.fill(Qt::transparent);
    {
        QPainter painter(&disabled);
        painter.setOpacity(0.35);
        painter.drawPixmap(0, 0, normal);
    }

    QIcon icon;
    icon.addPixmap(normal, QIcon::Normal);
    icon.addPixmap(disabled, QIcon::Disabled);
    return icon;
}
