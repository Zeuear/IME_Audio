#ifndef THEMEDICON_H
#define THEMEDICON_H

#include <QColor>
#include <QIcon>
#include <QString>

// @brief 单色描边 SVG 图标按当前主题文字色重新着色。
//        SVG 里用 currentColor 占位，每个图标只存一份，不必像 combo_down_* 那样按主题复制三份。
//        resource 是 res.qrc 里的别名（如 "snippet_add"）；logicalSize 为逻辑像素，内部按屏幕缩放比渲染。
QIcon themedIcon(const QString& resource, const QColor& color, int logicalSize = 18);

#endif // THEMEDICON_H
