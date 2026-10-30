#include "ui/controls/ThemedIcon.h"

#include <QBuffer>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QImageReader>
#include <QPainter>
#include <QPixmap>
#include <cmath>

#include "utils/Logger.h"

namespace {

QPixmap renderSvg(const QByteArray& svg, int logicalSize) {
    const qreal dpr = std::ceil(qApp->devicePixelRatio());
    const int px = static_cast<int>(logicalSize * dpr);

    QByteArray data = svg;
    QBuffer buffer(&data);
    buffer.open(QIODevice::ReadOnly);
    QImageReader reader(&buffer, "svg");
    reader.setScaledSize(QSize(px, px));
    const QImage image = reader.read();
    if (image.isNull()) {
        LOG_WARN(QString("Themed icon render failed: %1").arg(reader.errorString()));
        return {};
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
