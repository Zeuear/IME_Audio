#pragma once
#include <QColor>
#include <QImage>
#include <QPalette>
#include <QString>

// @brief 横条图标取色与渲染：线条 SVG 的 currentColor 换成目标色，主题和状态只靠调色板。
namespace ToolIcon {

enum class State {
    Off,
    On,
    Normal,
    Record,
    Disabled,
};

QColor stateColor(const QPalette& palette, State state);

QImage render(const QString& path, const QColor& color, int size);

// 带底板的图标，仿 macOS / iOS 凸起圆角方块；size 为像素边长（按 28 逻辑像素设计稿缩放）。
// accent 有效：彩色底 + 白色图案，用于"开启 / 进行中"；accent 无效：随主题的中性底 + glyph 色图案。
QImage renderTile(const QString& path, int size, const QPalette& palette,
                  const QColor& accent, const QColor& glyph, bool hover);

}
