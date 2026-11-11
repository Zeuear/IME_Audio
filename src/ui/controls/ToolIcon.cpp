#include "ToolIcon.h"
#include <QFile>
#include <QLinearGradient>
#include <QPainter>
#include <QSvgRenderer>

namespace ToolIcon {

QColor stateColor(const QPalette& palette, State state)
{
    switch (state) {
    case State::On:       return palette.color(QPalette::Active, QPalette::Highlight);
    case State::Normal:   return palette.color(QPalette::Active, QPalette::ButtonText);
    case State::Record:   return QColor(0xe5, 0x48, 0x4d);
    case State::Disabled: return palette.color(QPalette::Disabled, QPalette::ButtonText);
    case State::Off:      break;
    }
    return palette.color(QPalette::Active, QPalette::PlaceholderText);
}

QImage render(const QString& path, const QColor& color, int size)
{
    QFile file(path);
    if (size <= 0 || !file.open(QIODevice::ReadOnly)) return {};

    QByteArray svg = file.readAll();
    svg.replace("currentColor", color.name(QColor::HexRgb).toUtf8());

    QSvgRenderer renderer(svg);
    if (!renderer.isValid()) return {};

    QImage image(size, size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    renderer.render(&painter);
    return image;
}

QImage renderTile(const QString& path, int size, const QPalette& palette,
                  const QColor& accent, const QColor& glyph, bool hover)
{
    if (size <= 0) return {};
    QImage image(size, size, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter p(&image);
    p.setRenderHint(QPainter::Antialiasing);

    const qreal s = size / 28.0;                 // 设计稿 28px，其余尺寸等比
    const qreal side = size - 3 * s;             // 底部留 1 个单位给投影线
    const QRectF tile((size - side) / 2, s, side, side);
    const qreal radius = 8 * s;
    const bool colored = accent.isValid();
    const bool dark = palette.color(QPalette::Window).lightness() < 128;

    // 1. 底边一线投影，让方块"浮"起来
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, dark ? 90 : 38));
    p.drawRoundedRect(tile.translated(0, s), radius, radius);

    // 2. 渐变底
    QColor top, bottom;
    if (colored) {
        top = accent.lighter(hover ? 125 : 112);
        bottom = accent.darker(hover ? 100 : 112);
    } else if (dark) {
        top = QColor(255, 255, 255, hover ? 58 : 40);
        bottom = QColor(255, 255, 255, hover ? 30 : 14);
    } else {
        top = QColor(255, 255, 255, 255);
        bottom = hover ? QColor(236, 238, 242) : QColor(224, 227, 232);
    }
    QLinearGradient fill(tile.topLeft(), tile.bottomLeft());
    fill.setColorAt(0, top);
    fill.setColorAt(1, bottom);
    p.setBrush(fill);
    p.drawRoundedRect(tile, radius, radius);

    // 3. 内缘：顶部亮、底部暗，中段透明，立体感主要来自这一圈
    const qreal edgeInset = 1.0 * s;
    QLinearGradient edge(tile.topLeft(), tile.bottomLeft());
    edge.setColorAt(0.0, colored ? QColor(255, 255, 255, 110) : QColor(255, 255, 255, dark ? 60 : 255));
    edge.setColorAt(0.4, QColor(255, 255, 255, 0));
    edge.setColorAt(0.65, QColor(0, 0, 0, 0));
    edge.setColorAt(1.0, QColor(0, 0, 0, colored ? 55 : (dark ? 70 : 22)));
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QBrush(edge), 1.0 * s));
    p.drawRoundedRect(tile.adjusted(edgeInset, edgeInset, -edgeInset, -edgeInset) , radius - edgeInset, radius - edgeInset);

    // 4. 外描边，压住渐变的边缘
    QColor outline = colored ? accent.darker(160) : (dark ? QColor(255, 255, 255, 36) : QColor(0, 0, 0, 46));
    outline.setAlpha(colored ? 150 : outline.alpha());
    p.setPen(QPen(outline, 0.8 * s));
    p.drawRoundedRect(tile.adjusted(0.4 * s, 0.4 * s, -0.4 * s, -0.4 * s), radius, radius);

    // 5. 图案：先压一层阴影，再画本体
    const int glyphSize = qRound(19 * s);
    const QPointF origin(tile.center().x() - glyphSize / 2.0, tile.center().y() - glyphSize / 2.0);
    const QColor ink = colored ? QColor(Qt::white) : glyph;
    const QImage shadow = render(path, colored ? QColor(0, 0, 0) : (dark ? QColor(0, 0, 0) : QColor(255, 255, 255)), glyphSize);
    p.setOpacity(colored ? 0.30 : (dark ? 0.55 : 0.85));
    p.drawImage(origin + QPointF(0, colored ? s : (dark ? -s : s)), shadow);
    p.setOpacity(1.0);
    p.drawImage(origin, render(path, ink, glyphSize));
    return image;
}

} // namespace ToolIcon
