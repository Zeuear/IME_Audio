#include "ToolIcon.h"
#include <QFile>
#include <QPainter>
#include <QSvgRenderer>

namespace ToolIcon {

QColor stateColor(const QPalette& palette, State state)
{
    switch (state) {
    case State::On:       return palette.color(QPalette::Active, QPalette::Highlight);
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

} // namespace ToolIcon
