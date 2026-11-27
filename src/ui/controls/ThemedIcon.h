#ifndef THEMEDICON_H
#define THEMEDICON_H

#include <QColor>
#include <QIcon>
#include <QString>

// @brief 单色描边 SVG 图标按主题文字色重着色：SVG 用 currentColor 占位，每个图标只存一份。
QIcon themedIcon(const QString& resource, const QColor& color, int logicalSize = 18);

#endif // THEMEDICON_H
